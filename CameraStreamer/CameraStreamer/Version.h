#pragma once

// todo: pull application version from git
const unsigned int VERSION_MAJOR = 0; // breaking changes
const unsigned int VERSION_MINOR = 9;
const unsigned int VERSION_PATCH = 9;

// 0.9.9
// TCPRelayCamera
// - introducing a relay camera for RAW YUV420 network packets
//
// App status:
// - reporting streaming color or depth will be disabled if not supported by the camera

// 0.9.8
// Configuration
// - Reports missing configuration fields with more context. e.g., "camera.type" instead of "type"
// 
// VideoRecorder
// - Fixed bug that would prevent recording from working

// 0.9.7
// Camera:
// - Fixed camera thread loop bug (crashing after stopping the camera)
// - Added a way of exporting camera matrix (something that can be readily used by OpenCV)
// 
// ApplicationStatus && Controll server:
// - Reporting camera intrinsic matrix in the pong message
//
// Renaming application to CameraStreamer (issue 9)
//

// 0.9.6
// Remote Server:
// - Fixed ping message bug
//
// Configuration:
// - reporting streamer's protocols fixed (raw16 and jpeg)

// 0.9.5
// RealSense:
// - Not crashing when user-defined serial number is not present
//
// Configuration:
// - Reading user defined name for the camera (camera.name)
//
// AppStatus:
// - Reporting user defined name for the camera

// 0.9.4
// RealSense:
// - Fixing bug where RealSense was selecting the first camera despite having been told to use a specific camera
// 
// Camera:
// - Letting user know that a specific SN was specified in the configuration file
//
// Misc:
// - Making first logs less confusing

// 0.9.3
// Misc:
// - Moving log strings such as "AzureKinect" and "RealSense2" to constant strings declared only once
//
// RealSense:
// - RealSense will wait less than 15 seconds before timing out (defaults to 1000ms)
//
// Configuration:
// - Added a new parameter: camera.frameTimeoutMS to set frame capture timeout

