#pragma once


#include <ostream>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <string>
#include <sstream>

// This class' only purpose is to make logging with datetime accessible to 
// all members of this software piece
class Logger
{

public:
	static std::ostream& Log(const std::string& module)
	{
		// gets current time
		struct tm buf;
		time_t t = time(nullptr);
		localtime_s(&buf, &t);

		// prints to the console
		return (std::cout << std::put_time(&buf, "%m/%d/%Y %H:%M:%S ") << '[' << std::setfill(' ') << std::setw(11) << module << "] - ");
	}
};