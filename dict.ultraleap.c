//
// ultraleap
//
// Max Object for Leap Motion V3 (new API) Skeletal tracking in Max on Apple Silicon Machines
// Only compiles for arm64 target
// Requires having ultraleap software installed
//
// author: Andrew Benson
// contact: pixlpa@gmail.com
//

#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object
#include "ext_proto.h"
#include "ext_systhread.h"
#include "ext_strings.h"
#include "ext_dictobj.h"

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "LeapC.h"

// a macro to mark exported symbols in the code without requiring an external file to define them
#ifdef WIN_VERSION
// note that this is the required syntax on windows regardless of whether the compiler is msvc or gcc
#define T_EXPORT __declspec(dllexport)
#else // MAC_VERSION
// the mac uses the standard gcc syntax, you should also set the -fvisibility=hidden flag to hide the non-marked symbols
#define T_EXPORT __attribute__((visibility("default")))
#endif

////////////////////////// object struct
typedef struct _dict_ultraleap
{
	t_object ob;
    void *outlet_frame;
    void *outlet_start;
    t_symbol        *name;            // symbol mapped to the dictionary
    t_dictionary    *dictionary;    // the actual dictionary
    bool isrunning;
    t_int64 lastframeid;
    LEAP_CONNECTION connection;
    LEAP_CLOCK_REBASER clockSynchronizer;
    LEAP_TRACKING_EVENT *frame;
    t_systhread        x_systhread;                        // thread reference
    t_systhread_mutex    x_mutex;                            // mutual exclusion lock for threadsafety
    int                x_systhread_cancel;                    // thread cancel flag
    void                *x_qelem;
    t_int frame_id_save;
} t_dict_ultraleap;

///////////////////////// function prototypes
//// standard set
void *dict_ultraleap_new(t_symbol *s, long argc, t_atom *argv);
void dict_ultraleap_free(t_dict_ultraleap *x);
void dict_ultraleap_assist(t_dict_ultraleap *x, void *b, long m, long a, char *s);
void dict_ultraleap_bang(t_dict_ultraleap *x);
void dict_ultraleap_connect(t_dict_ultraleap *x);
void dict_ultraleap_stop(t_dict_ultraleap *x);
void dict_ultraleap_systhread_start(t_dict_ultraleap *x);
void *dict_ultraleap_service(t_dict_ultraleap *x);
void * dict_ultraleap_tick(t_dict_ultraleap *x);
void dict_ultraleap_setname(t_dict_ultraleap *x, void *attr, long argc, t_atom *argv);

// class statics/globals
t_symbol *ps_name;
static t_symbol *ps_modified;
static t_symbol *ps_dictionary;

//global class pointer variable
void *dict_ultraleap_class;

//Max functions
int T_EXPORT main(void)
{
	t_class *c;
	
	c = class_new("dict.ultraleap", (method)dict_ultraleap_new, (method)dict_ultraleap_free, (long)sizeof(t_dict_ultraleap), 0L /* leave NULL!! */, A_GIMME, 0);
	
    class_addmethod(c, (method)dict_ultraleap_bang, "bang", 0);
    class_addmethod(c, (method)dict_ultraleap_connect, "connect", 0);
    class_addmethod(c, (method)dict_ultraleap_stop, "stop", 0);
    class_addmethod(c, (method)dict_ultraleap_assist, "assist", A_CANT, 0);
    
    CLASS_ATTR_SYM(c,            "name",            0, t_dict_ultraleap, name);
    CLASS_ATTR_ACCESSORS(c,        "name",            NULL, dict_ultraleap_setname);
    CLASS_ATTR_CATEGORY(c,        "name",            0, "Dictionary");
    CLASS_ATTR_LABEL(c,            "name",            0, "Name");
    CLASS_ATTR_BASIC(c,            "name",            0);
	
	class_register(CLASS_BOX, c);
	dict_ultraleap_class = c;
    
    ps_name = gensym("name");
    ps_modified = gensym("modified");
    ps_dictionary = gensym("dictionary");
    
	return 0;
}

void dict_ultraleap_assist(t_dict_ultraleap *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "bang to cause the frame data output");
	}
	else {	// outlet
		switch (a) {
            case 0:
				sprintf(s, "Dictionary Output");
				break;
            case 1:
                sprintf(s, "Begin Frame");
			default:
				break;
		}
	}
}

// Initializes the connection to Leap and starts the message service thread
void dict_ultraleap_connect(t_dict_ultraleap *x){
    post("trying to connect");
    if(x->isrunning){
        post("already connected!");
      return &x->connection;
    }
    eLeapRS result;
    if(x->connection || LeapCreateConnection(NULL, &x->connection) == eLeapRS_Success){
        result = LeapOpenConnection(x->connection);
        if(result == eLeapRS_Success){
            x->isrunning = true;
            dict_ultraleap_systhread_start(x);
            post("Leap Connected");
        }
        else post("Leap connection not opened");
    }
    else post("problem with creating connection");
    return &x->connection;
}

void dict_ultraleap_free(t_dict_ultraleap *x)
{
    dict_ultraleap_stop(x); // stop the service thread
    LeapCloseConnection(x->connection); // close the leap connection
    LeapDestroyConnection(x->connection); // destroy the leap connection
    // free our mutex
    if (x->x_mutex)
        systhread_mutex_free(x->x_mutex);
    object_free((t_object *)x->dictionary); // will call object_unregister
}

//worker thread function that polls the Leap service and stores tracking frames
void * dict_ultraleap_tick(t_dict_ultraleap *x){
    while(1){
        if (x->x_systhread_cancel)
            return NULL;
        eLeapRS result;
        LEAP_CONNECTION_MESSAGE msg;
        LEAP_CONNECTION_INFO *cInfo;
        if(x->isrunning){
            //post("running");
            unsigned int timeout = 10;
            result = LeapPollConnection(x->connection, timeout, &msg);
            if(result == eLeapRS_Success){
                if (msg.type == eLeapEventType_Tracking){
                    LEAP_TRACKING_EVENT *frame = (LEAP_TRACKING_EVENT *)msg.tracking_event;
                    //use mutex lock to prevent issues between threads
                    systhread_mutex_lock(x->x_mutex);
                    if(!x->frame) x->frame = (LEAP_TRACKING_EVENT *)sysmem_newptr((t_ptr_size)(sizeof(*frame)));
                        *x->frame = *frame; // store frame of data in object struct
                        systhread_mutex_unlock(x->x_mutex);
                }
            }
            //else post("not able to poll");
        }
        //systhread_sleep(1);
    }
    x->x_systhread_cancel = false;
    systhread_exit(0);
    return NULL;
}

//start the worker thread
void dict_ultraleap_systhread_start(t_dict_ultraleap *x){
    unsigned int ret;
    if (x->x_systhread) {
        x->x_systhread_cancel = true;                        // tell the thread to stop
        systhread_join(x->x_systhread, &ret);                    // wait for the thread to stop
        x->x_systhread = NULL;
    }
    if (x->x_systhread == NULL) {
        systhread_create((method) dict_ultraleap_tick, x, 0, 0, 0, &x->x_systhread);
    }
}

void dict_ultraleap_stop(t_dict_ultraleap *x)
{
    unsigned int ret;

    if (x->x_systhread) {
        post("stopping leap service");
        x->x_systhread_cancel = true;                        // tell the thread to stop
        systhread_join(x->x_systhread, &ret);                    // wait for the thread to stop
        x->x_systhread = NULL;
    }
}

//read from the most recent frame of data received from Leap
void dict_ultraleap_bang(t_dict_ultraleap *x)
{
    if(x->isrunning){
        LEAP_TRACKING_EVENT *frame;
        systhread_mutex_lock(x->x_mutex);
        frame = x->frame;
        systhread_mutex_unlock(x->x_mutex);
        if (frame){
            int64_t frameID = frame->tracking_frame_id;
            if(frameID != x->lastframeid){
                dictionary_clear(x->dictionary);
                dictionary_appendlong(x->dictionary,gensym("id"),frameID);
                t_atom frame_data[5];
                atom_setsym(frame_data, gensym("id"));
                atom_setlong(frame_data+1, frameID);
                t_int numhands = (t_int) frame->nHands;
                dictionary_appendlong(x->dictionary,gensym("numhands"), numhands);
                t_dictionary *hand_dict[2];
                outlet_bang(x->outlet_start);
                for(uint32_t h = 0; h < numhands; h++){
                    LEAP_HAND* hand = &frame->pHands[h];
                    t_atom hand_data[11];
                    // palmPosition
                    t_symbol *hand_type = (hand->type == eLeapHandType_Left) ? gensym("left") : gensym("right");
                    hand_dict[h] = dictionary_new();
                    atom_setsym(hand_data, hand_type);
                    //set palm position values
                    atom_setfloat(hand_data+1, hand->palm.position.x);
                    atom_setfloat(hand_data+2, hand->palm.position.y);
                    atom_setfloat(hand_data+3, hand->palm.position.z);
                    dictionary_appendatoms(hand_dict[h],gensym("position"),3, hand_data+1);
                    //outlet_list(x->outlet_hands, NULL, 4, hand_data);
                    //set orientation values
                    atom_setfloat(hand_data+1, hand->palm.orientation.v[0]);
                    atom_setfloat(hand_data+2, hand->palm.orientation.v[1]);
                    atom_setfloat(hand_data+3, hand->palm.orientation.v[2]);
                    atom_setfloat(hand_data+4, hand->palm.orientation.v[3]);
                    dictionary_appendatoms(hand_dict[h],gensym("orientation"),4, hand_data+1);
                    //set normal values
                    atom_setfloat(hand_data+1, hand->palm.normal.x);
                    atom_setfloat(hand_data+2, hand->palm.normal.y);
                    atom_setfloat(hand_data+3, hand->palm.normal.z);
                    dictionary_appendatoms(hand_dict[h],gensym("normal"),3, hand_data+1);
                    //set direction values
                    atom_setfloat(hand_data+1, hand->palm.direction.x);
                    atom_setfloat(hand_data+2, hand->palm.direction.y);
                    atom_setfloat(hand_data+3, hand->palm.direction.z);
                    dictionary_appendatoms(hand_dict[h],gensym("direction"),3, hand_data+1);
                    t_dictionary *this_finger[5];
                    char *fingernames[5] = {"thumb","index","middle","ring","pinky"};
                    for(t_int f = 0; f < 5; f++){
                        LEAP_DIGIT* finger = &hand->digits[f];
                        t_atom finger_data[4];
                        atom_setlong(finger_data,f);
                        atom_setfloat(finger_data+1, finger->bones[3].next_joint.x);
                        atom_setfloat(finger_data+2, finger->bones[3].next_joint.y);
                        atom_setfloat(finger_data+3, finger->bones[3].next_joint.z);
                        //outlet_list(x->outlet_fingers, NULL, 4, finger_data);
                        this_finger[f] = dictionary_sprintf("@base %2f %2f %2f @joint1 %2f %2f %2f @joint2 %2f %2f %2f @tip %2f %2f %2f",  finger->bones[0].next_joint.x, finger->bones[0].next_joint.y, finger->bones[0].next_joint.z,  finger->bones[1].next_joint.x, finger->bones[1].next_joint.y, finger->bones[1].next_joint.z, finger->bones[2].next_joint.x, finger->bones[2].next_joint.y, finger->bones[2].next_joint.z, finger->bones[3].next_joint.x, finger->bones[3].next_joint.y, finger->bones[3].next_joint.z);
                        dictionary_appenddictionary(hand_dict[h], gensym(fingernames[f]), (t_object *)this_finger[f]);
                    }
                    dictionary_appenddictionary(x->dictionary, hand_type, (t_object *)hand_dict[h]);
                }
                if (x->name) {
                    t_atom    a[1];
                    atom_setsym(a, x->name);
                    outlet_anything(x->outlet_frame, ps_dictionary, 1, a);
                }
            }
            x->lastframeid = frameID;
        }
    }
}

void dict_ultraleap_setname(t_dict_ultraleap *x, void *attr, long argc, t_atom *argv)
{
    t_symbol        *name = atom_getsym(argv);

    if (!x->name || !name || x->name!=name) {
        object_free(x->dictionary); // will call object_unregister
        x->dictionary = dictionary_new();
        x->dictionary = dictobj_register(x->dictionary, &name);
        x->name = name;
    }
    if (!x->dictionary)
        object_error((t_object *)x, "could not create dictionary named %s", name->s_name);
}

void *dict_ultraleap_new(t_symbol *s, long argc, t_atom *argv)
{
	t_dict_ultraleap *x = NULL;
	if ((x = (t_dict_ultraleap *)object_alloc((t_class *)dict_ultraleap_class)))
	{
        long            attrstart = attr_args_offset(argc, argv);
        t_symbol        *name = NULL;
        if (attrstart && atom_gettype(argv) == A_SYM)
            name = atom_getsym(argv);
        x->frame_id_save = 0;
        x->outlet_start = outlet_new(x, NULL);
        x->outlet_frame = outlet_new(x, NULL);
        x->dictionary = dictionary_new();
        attr_args_process(x, argc, argv);
        if (!x->name) {
            if (name)
                object_attr_setsym(x, ps_name, name);
            else
                object_attr_setsym(x, ps_name, symbol_unique());
        }
        LeapCreateClockRebaser(&x->clockSynchronizer);
        x->x_systhread = NULL;
        systhread_mutex_new(&x->x_mutex,0);
	}
	return (x);
}
