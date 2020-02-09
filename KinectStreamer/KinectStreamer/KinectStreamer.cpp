// KinectStreamer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <k4a/k4a.h>
#include "TCPServer.h"
#include "Logger.h"

using namespace std;
int main()
{
	
	Logger::Log("Main") << "There are " << k4a_device_get_installed_count() << " kinect devices connected to this computer" << endl;

	// no devices installed ?
	if (k4a_device_get_installed_count() == 0)
	{
		Logger::Log("Main") << "No AzureKinect devices connected ... exiting" << endl;
		return 1;
	}

	// main application loop where it waits for a user key to stop everything
	{
		TCPServer server(27015);
		server.Run();

		// starts kinect azure


		std::cout << endl;
		Logger::Log("Main") << "To close this application, press 'q'" << endl;

		// waits for user command to exit
		while (getchar() != 'q')
		{
			
		}
		Logger::Log("Main") << "User pressed 'q'. Exiting... " << endl;
	}


}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
