#pragma once

#include <string>

#include "permission.hpp"

struct session_data
{
	bool              logged_in = false;
	long long         user_id   = 0;
	std::string       username;
	std::string       display_name;
	permission::flags permissions;
};