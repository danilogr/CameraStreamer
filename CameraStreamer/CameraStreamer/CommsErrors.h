#pragma once

#include <boost/system/error_code.hpp>
#include <iostream>
#include <string>

namespace comms
{
	enum class error
	{
		Success = 0,
		TimedOut = 1,
		Cancelled = 2
	};
}

namespace boost
{
	namespace system
	{
		// Tell the C++ 11 STL metaprogramming that enum comms:error
		// is registerd with the standard error code system
		template <> struct is_error_code_enum<comms::error> : std::true_type
		{

		};
	}
}

namespace detail
{
	// Define a custom error code category derived from boost::system::error_category
	class CommsError_category : public boost::system::error_category
	{
	public:

		// return a short descriptive name for the category
		virtual const char* name() const noexcept override final { return "CommsError"; }

		// return what each enum means in text
		virtual std::string message(int c) const override final
		{
			switch (static_cast<comms::error>(c))
			{
				case comms::error::Success:
					return "operation successful";
				case comms::error::TimedOut:
					return "operation timed out";
				case comms::error::Cancelled:
					return "operation cancelled by the user";
				default:
					return "unknown";
					
			}
		}

		// OPTIONAL: Allow generic error conditions to be compared to me
		virtual boost::system::error_condition default_error_condition(int c) const noexcept override final
		{
			switch (static_cast<comms::error>(c))
			{
			case comms::error::Success:
				return make_error_condition(boost::system::errc::success);
			case comms::error::TimedOut:
				return make_error_condition(boost::system::errc::timed_out);
			case comms::error::Cancelled:
				return make_error_condition(boost::system::errc::operation_canceled);
			default:
				// I have no mapping for this code
				return boost::system::error_condition(c, *this);
			}
		}

	};
}


// Define the linkage for this function to be used by external code.
// This would be the usual __declspec(dllexport) or __declspec(dllimport)
// if we were in a Windows DLL etc. But for this example use a global
// instance but with inline linkage so multiple definitions do not collide.
#define THIS_MODULE_API_DECL extern inline

// Declare a global function returning a static instance of the custom category
THIS_MODULE_API_DECL const detail::CommsError_category& CommsError_category()
{
	static detail::CommsError_category c;
	return c;
}


// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
// see https://stackoverflow.com/questions/39775306/make-error-code-was-not-declared-in-this-scope-and-no-declarations-were-found

namespace comms
{
	inline boost::system::error_code make_error_code(comms::error e)
	{
		return { static_cast<int>(e), CommsError_category() };
	}
}