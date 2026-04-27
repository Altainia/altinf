#pragma once

#include "permission.hpp"

#include <string>

struct session_data
{
	bool              logged_in   = false;
	std::string       username;
	std::string       display_name;
	permission::flags permissions;
};
