# ultraleap-for-max
 A Max object for using Ultraleap Leap Motion with Apple Silicon computers.
 
 Requires latest Ultraleap Software installed, which will add the SDK files inside of the Application bundle. In addition to all the usual Max-SDK settings, you will need to go into the project settings and add the dylib filepath to Other Linker Flags:
 `-Wl,-headerpad_max_install_names`
 `"/Applications/Ultraleap\ Hand\ Tracking\ Service.app/Contents/LeapSDK/lib/libLeapC.5.dylib"`
 And also add the header folder to the Header Search Paths `"/Applications/Ultraleap\ Hand\ Tracking\ Service.app/Contents/LeapSDK/include"` (I also added to System Header Search Paths)
 I also added the "lib" path to Library Search Paths for good measure.
 It's a little bit difficult to make sure all the things are getting picked up by Xcode, but you can also look at the Cmake files included in their LeapSDK/examples/ folder or run Cmake on that folder to build a project you can reference.
 To download the tracking service: https://developer.leapmotion.com/tracking-software-download
 
 Also, make sure that the project can find the max-sdk/c74support files as well. If you've developed externals before, you'll know how to do this.'
 * Note that this is only set up to run on Arm64 Mac computers, so I only build the arm64 target. The source code could probably be extended to build on Windows using their Gemini dll as well"*"
