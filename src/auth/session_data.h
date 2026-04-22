#pragma once

#include <cstdint>
#include <string>

struct session_data
{
	bool        logged_in = false;
	std::string username;
	uint64_t    permissions = 0;
};
