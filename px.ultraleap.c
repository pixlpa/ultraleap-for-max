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
typedef struct _ultraleap
{
	t_object ob;
	void *outlet_end;
    void *outlet_fingers;
    void *outlet_hands;
    void *outlet_frame;
    void *outlet_start;
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
} t_ultraleap;

///////////////////////// function prototypes
//// standard set
void *ultraleap_new(t_symbol *s, long argc, t_atom *argv);
void ultraleap_free(t_ultraleap *x);
void ultraleap_assist(t_ultraleap *x, void *b, long m, long a, char *s);
void ultraleap_bang(t_ultraleap *x);
void ultraleap_connect(t_ultraleap *x);
void ultraleap_stop(t_ultraleap *x);
void ultraleap_systhread_start(t_ultraleap *x);
void *ultraleap_service(t_ultraleap *x);
void * ultraleap_tick(t_ultraleap *x);

//////////////////////// global class pointer variable
void *ultraleap_class;

//////////////////////// Max functions
int T_EXPORT main(void)
{
	t_class *c;
	
	c = class_new("px.ultraleap", (method)ultraleap_new, (method)ultraleap_free, (long)sizeof(t_ultraleap), 0L /* leave NULL!! */, A_GIMME, 0);
	
    class_addmethod(c, (method)ultraleap_bang, "bang", 0);
    class_addmethod(c, (method)ultraleap_connect, "connect", 0);
    class_addmethod(c, (method)ultraleap_stop, "stop", 0);
    //class_addmethod(c, (method)ultraleap_systhread_start, "start", 0);
    
	/* you CAN'T call this from the patcher */
    class_addmethod(c, (method)ultraleap_assist, "assist", A_CANT, 0);
	
	class_register(CLASS_BOX, c);
	ultraleap_class = c;
    
	return 0;
}

void ultraleap_assist(t_ultraleap *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "bang to cause the frame data output");
	}
	else {	// outlet
		switch (a) {
			case 0:
				sprintf(s, "End Frame");
				break;
            case 1:
				sprintf(s, "Fingers");
				break;
            case 2:
				sprintf(s, "Hands");
				break;
            case 3:
				sprintf(s, "Begin Frame");
				break;
            case 4:
                sprintf(s, "Frame");
			default:
				break;
		}
	}
}

// Initializes the connection to Leap and starts the message service thread
void ultraleap_connect(t_ultraleap *x){
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
            ultraleap_systhread_start(x);
            post("Leap Connected");
        }
        else post("Leap connection not opened");
    }
    else post("problem with creating connection");
    return &x->connection;
}

void ultraleap_free(t_ultraleap *x)
{
    ultraleap_stop(x); // stop the service thread
    LeapCloseConnection(x->connection); // close the leap connection
    LeapDestroyConnection(x->connection); // destroy the leap connection
    // free our mutex
    if (x->x_mutex)
        systhread_mutex_free(x->x_mutex);
}

//worker thread function that polls the Leap service and stores tracking frames
void * ultraleap_tick(t_ultraleap *x){
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
void ultraleap_systhread_start(t_ultraleap *x){
    unsigned int ret;
    if (x->x_systhread) {
        x->x_systhread_cancel = true;                        // tell the thread to stop
        systhread_join(x->x_systhread, &ret);                    // wait for the thread to stop
        x->x_systhread = NULL;
    }
    if (x->x_systhread == NULL) {
        systhread_create((method) ultraleap_tick, x, 0, 0, 0, &x->x_systhread);
    }
}

void ultraleap_stop(t_ultraleap *x)
{
    unsigned int ret;

    if (x->x_systhread) {
        post("stopping leap service");
        x->x_systhread_cancel = true;                        // tell the thread to stop
        systhread_join(x->x_systhread, &ret);                    // wait for the thread to stop
        x->x_systhread = NULL;
    }
}

void simplethread_cancel(t_ultraleap *x)
{
    ultraleap_stop(x);                                    // kill thread if, any
}

//read from the most recent frame of data received from Leap
void ultraleap_bang(t_ultraleap *x)
{
    if(x->isrunning){
        LEAP_TRACKING_EVENT *frame;
        systhread_mutex_lock(x->x_mutex);
        frame = x->frame;
        systhread_mutex_unlock(x->x_mutex);
        if (frame){
            int64_t frameID = frame->tracking_frame_id;
            if(frameID != x->lastframeid){
                t_atom frame_data[5];
                atom_setsym(frame_data, gensym("id"));
                atom_setlong(frame_data+1, frameID);
                t_int numhands = (t_int) frame->nHands;
                if(numhands>0) outlet_bang(x->outlet_start);
                for(uint32_t h = 0; h < numhands; h++){
                    LEAP_HAND* hand = &frame->pHands[h];
                    t_atom hand_data[11];
                    // palmPosition
                    t_symbol *hand_type = (hand->type == eLeapHandType_Left) ? gensym("left") : gensym("right");
                    atom_setsym(hand_data, hand_type);
                    atom_setfloat(hand_data+1, hand->palm.position.x);
                    atom_setfloat(hand_data+2, hand->palm.position.y);
                    atom_setfloat(hand_data+3, hand->palm.position.z);
                    outlet_list(x->outlet_hands, NULL, 4, hand_data);
                    for(t_int f = 0; f < 5; f++){
                        LEAP_DIGIT* finger = &hand->digits[f];
                        t_atom finger_data[4];
                        atom_setlong(finger_data,f);
                        atom_setfloat(finger_data+1, finger->bones[3].next_joint.x);
                        atom_setfloat(finger_data+2, finger->bones[3].next_joint.y);
                        atom_setfloat(finger_data+3, finger->bones[3].next_joint.z);
                        outlet_list(x->outlet_fingers, NULL, 4, finger_data);
                    }
                }
                if (numhands>0) outlet_bang(x->outlet_end);
            }
            x->lastframeid = frameID;
        }
        //else post("frame failed");
    }
    //else post("not running");
}

void *ultraleap_new(t_symbol *s, long argc, t_atom *argv)
{
	t_ultraleap *x = NULL;
    
	if ((x = (t_ultraleap *)object_alloc((t_class *)ultraleap_class)))
	{
        x->frame_id_save = 0;
        x->outlet_start = outlet_new(x, NULL);
        x->outlet_frame = outlet_new(x, NULL);
		x->outlet_hands = outlet_new(x, NULL);
        x->outlet_fingers = outlet_new(x, NULL);
        x->outlet_end = outlet_new(x, NULL);

        LeapCreateClockRebaser(&x->clockSynchronizer);
        x->x_systhread = NULL;
        systhread_mutex_new(&x->x_mutex,0);
	}
	return (x);
}
