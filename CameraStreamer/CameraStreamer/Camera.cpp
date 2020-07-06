#include "Camera.h"

#include <sstream>

std::string Camera::OpenCVCameraMatrix(const CameraParameters& param) const
{
	std::stringstream ss;

	// camera matrix intrinsics
	ss << "{\"camera_matrix\": [";
	ss << '[' << param.intrinsics.fx << ", 0.0,"			<<        param.intrinsics.cx << "],";
	ss << "[0.0, "				 << param.intrinsics.fy     <<", " << param.intrinsics.cy << "],";
	ss << "[0.0, 0.0, 1.0]],";

	// camera matrix distortion coefficients
	// [k1,k2,p1,p2,k3,k4,k5,k6,s1,s2,s3,s4,taux,tauy] 
	ss << "\"dist_coeff\" : [[";
	ss << param.intrinsics.k1 << ", ";
	ss << param.intrinsics.k2 << ", ";
	ss << param.intrinsics.p1 << ", ";
	ss << param.intrinsics.p2 << ", ";
	ss << param.intrinsics.k3 << ", ";
	ss << param.intrinsics.k4 << ", ";
	ss << param.intrinsics.k5 << ", ";
	ss << param.intrinsics.k6 << "]],  \"mean_error\" : 0.00}"; // error is really unknown

	return ss.str();
}