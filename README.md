# ultraleap-for-max
 A Max object for using Ultraleap Leap Motion with Apple Silicon computers.
 
 Requires latest Ultraleap Software installed, which will add the SDK files inside of the Application bundle. The included CMakeLists file should generate the appropriate xcode settings.
 To download the tracking service: https://developer.leapmotion.com/tracking-software-download
 
 Also, make sure that the project can find the max-sdk/c74support files as well. If you've developed externals before, you'll know how to do this.'
 * Note that this is only set up to run on Arm64 Mac computers, so I only build the arm64 target. The source code could probably be extended to build on Windows using their Gemini dll as well"*"
