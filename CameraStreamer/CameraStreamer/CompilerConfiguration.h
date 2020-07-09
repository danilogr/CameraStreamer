#pragma once

// 
//  Use this header to enable or disable features. In the future,
//  we might have camera modules compiled to DLLs, but for now
//  we are deciding which features ship with the application during
//  compilation time
// 


#define ENABLE_K4A 1 // azure kinect cameras
#define ENABLE_RS2 1 // real sense api

#define ENABLE_REPLAY_CAMERA 1 // camera that can help measuring latency