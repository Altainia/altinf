#pragma once

#include <Wt/WDate.h>

#include <string>
#include <vector>

struct blog_post
{
	std::string              title;
	Wt::WDate                date;
	std::vector<std::string> tags;
	std::string              slug;
	std::string              filepath;
};
