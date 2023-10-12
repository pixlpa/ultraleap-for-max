# ultraleap-for-max
 ##A Max object for using Ultraleap Leap Motion with Apple Silicon computers.
 
 *Mathieu Chamagne is developing a more actively maintained [ultraleap object](https://github.com/celtera/ultraleap/tree/main) as well. I'd recommend following that and contributing to their fund for more reliable results.*
 ##Info
 This project was begun primarily to experiment with the new Gemini SDK from Ultraleap and to revive a Max-based live performance project that relies heavily on Leap Motion. It has also been awhile since I wrote a Max external in C, and wanted to refresh my memory. It was a good excuse to learn about background threads, mutexes, and dictionaries.'
 ##px.ultraleap
 A simple C object that connects to a Leap Motion device, parses tracking frames continuously, and only outputs tracking data when sent a *bang*. It's loosely based on previous leapmotion externals, but ultimately had to be rewritten from scratch due to how different the newer Leap SDK is from the older one. This object only outputs palm positions and fingertip positions (it's all I needed), but might be extended with access to more data.
 ##px.dict.ultraleap
 Due to the extensive amount of data that must be managed with the hand tracking, I wanted to experiment with storing the tracking data in a dictionary instead. This object includes more of the provided data than the regular version, and is actually pretty nice to use.
 
 ##Building and Installing
 - Requires latest [Ultraleap Gemini Software](https://developer.leapmotion.com/tracking-software-download) installed, which will add the SDK files inside of the Application bundle. 
 - This project is made to be built with the max-sdk installed. I personally just add a folder to the max-sdk/source for each of the objects and copy the CmakeLists file into it, before running the Cmake *generate* command on the sdk folder.
 - The included CMakeLists file should generate the appropriate Xcode settings, but might need to have certain search paths added by hand afterwards. 
 - Make sure that the compiler is able to find the header files and dylib inside of the Contents/LeapSDK folder inside the Ultraleap Tracking Service app bundle. I'm not a CMake expert and have had to go back and fiddle with it repeatedly.

 * Note that this is only set up to run on Arm64 Mac computers, so I only build the arm64 target. The source code could probably be extended to build on Windows using their Gemini dll as well"*"
