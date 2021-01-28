#pragma once

// 
//  Use this header to enable or disable features. In the future,
//  we might have camera modules compiled to DLLs, but for now
//  we are deciding which features ship with the application during
//  compilation time
// 



#define CS_ENABLE_CAMERA_K4A 1					// azure kinect cameras (needs k4a:x64-windows)
#define CS_ENABLE_CAMERA_RS2 1					// real sense api       (needs realsense2:x64-windows)
#define CS_ENABLE_CAMERA_TCPCLIENT_RELAY 1		// camera that relays content from the network (TCP - better for local area network)
//#define CS_ENABLE_CAMERA_CV_VIDEOCAPTURE 1	    // using opencv to receive content from connected cameras

// ----  WIP ----  (Disabled for now as it is being developed)

//#define CS_ENABLE_CAMERA_VIDEOFILE 1			// video file replay camera (OpenCV)
//#define CS_ENABLE_CAMERA_K4AMKVPLAYER 1			// camera that plays MKV files created by a k4a tools

//#define CS_ENABLE_CAMERA_DVI2USB 1			    // DVI2USB camera (needs headers and libraries already packaged in with CameraStreamer)



