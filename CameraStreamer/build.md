# Building DataPipe

DataPipe relies on several open source projects such as [Azure Kinect](https://github.com/Microsoft/Azure-Kinect-Sensor-SDK), [OpenCV](https://github.com/opencv/opencv), and [boost](https://www.boost.org/).

Compiling and installing these libraries on Windows can be complicated and time consuming, so we rely on [vcpkg](https://github.com/microsoft/vcpkg) to manage all the required libraries.

After installing and integrating `vcpkg` through the instructions available [here](https://github.com/microsoft/vcpkg), you can install the required libraries with the following command:

`vcpkg install realsense2:x64-windows azure-kinect-sensor-sdk:x64-windows opencv:x64-windows boost:x64-windows rapidjson:x64-windows`
