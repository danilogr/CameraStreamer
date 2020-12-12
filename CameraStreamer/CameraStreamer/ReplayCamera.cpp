#include "ReplayCamera.h"


// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef CS_ENABLE_CAMERA_VIDEOFILE

// name used in logs
const char* ReplayCamera::ReplayCameraConstStr = "ReplayCamera";


void ReplayCamera::CameraLoop()
{

	Logger::Log(ReplayCameraConstStr) << "Started Replay Camera polling thread: " << std::this_thread::get_id << std::endl;

}

#endif

