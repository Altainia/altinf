#pragma once

#include <string>

#include "permission.hpp"

struct session_data
{
	bool              logged_in = false;
	std::string       username;
	std::string       display_name;
	permission::flags permissions;
};