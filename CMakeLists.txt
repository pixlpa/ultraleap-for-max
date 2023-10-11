include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################

set(CMAKE_OSX_ARCHITECTURES arm64)
set(ULTRALEAP_PATH_ROOT "/Applications/Ultraleap\ Hand\ Tracking\ Service.app/Contents/LeapSDK/")

find_package(LeapSDK
     5
     REQUIRED
     PATHS
	 	"${ULTRALEAP_PATH_ROOT}")
if (UNIX)    
    find_package(Threads REQUIRED)    
endif (UNIX)

include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
	"${ULTRALEAP_PATH_ROOT}/include"
)

file(GLOB PROJECT_SRC
     "*.h"
	 "*.c"
     "*.cpp"
"${ULTRALEAP_PATH_ROOT}/include/LeapC.h"
)
add_library( 
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

link_libraries(
	${PROJECT_NAME}
	PRIVATE
	"${ULTRALEAP_PATH_ROOT}/lib/libLeapC.5.dylib"
)
target_link_libraries(
	${PROJECT_NAME}
	PUBLIC
	"${ULTRALEAP_PATH_ROOT}/lib/libLeapC.5.dylib"
)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
