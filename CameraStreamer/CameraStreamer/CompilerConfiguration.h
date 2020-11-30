#pragma once

// 
//  Use this header to enable or disable features. In the future,
//  we might have camera modules compiled to DLLs, but for now
//  we are deciding which features ship with the application during
//  compilation time
// 



#define ENABLE_K4A 1					// azure kinect cameras (needs k4a:x64-windows)
#define ENABLE_RS2 1					// real sense api       (needs realsense2:x64-windows)
#define ENABLE_TCPCLIENT_RELAY_CAMERA 1 // camera that relays content from the network (TCP - better for local area network)


// ----  WIP ----  (Disabled for now as it is being developed)

#define ENABLE_REPLAY_CAMERA 1			// video file replay camera (OpenCV)
#define ENABLE_MKV_PLAYER 1				// camera that plays MKV files created by a k4a tools
#define ENABLE_OPENCV_WEBCAM 1		    // using opencv to receive content from connected cameras
#define ENABLE_DVI2USB_CAMERA 1		    // DVI2USB camera (needs headers and libraries already packaged in with CameraStreamer)


