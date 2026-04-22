#pragma once

#include <string>
#include <vector>

struct blog_post
{
	std::string              title;
	std::string              date;
	std::vector<std::string> tags;
	std::string              slug;
	std::string              filepath;
};
