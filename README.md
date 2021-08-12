# DataPipe?
Data acquisition, manipulation, integration, and storage is at the core of most interactive cross device (XD) prototypes.

DataPipe is a swiss-army knife tool that makes that easy!




## Building DataPipe
 - Install [vcpkg](https://github.com/microsoft/vcpkg#quick-start-windows)
 - Use `vcpkg` to install the following packages:
 - **boost** `vcpkg install boost:x64-windows`
 - [**OpenCV 4**](https://github.com/opencv/opencv)   `vcpkg install opencv4:x64-windows`
 - [**librealsense (rs2)**](https://github.com/IntelRealSense/librealsense)  `vcpkg install realsense2:x64-windows`
 - [**Kinect Azure SDK (k4a)**](https://github.com/microsoft/Azure-Kinect-Sensor-SDK)  `vcpkg install azure-kinect-sensor-sdk:x64-windows`
 - [**libjpeg-turbo**](https://libjpeg-turbo.org/)** `vcpkg install libjpeg-turbo:x64-windows`
 - [**https://rapidjson.org/**](https://rapidjson.org/) `vcpkg install rapidjson:x64-windows`

## Supported cameras
 - [Microsoft Kinect Azure](https://azure.microsoft.com/en-us/services/kinect-dk/)
 - [Intel RealSense 2](https://www.intel.com/content/www/us/en/architecture-and-technology/realsense-overview.html)


# License
<a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/">Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License</a>. If you are interested in using this tool commercially, reach out to commercial@xrxdprototyping.io

