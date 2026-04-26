#pragma once

#include <Wt/WDate.h>

#include <optional>
#include <string>
#include <vector>

struct blog_post
{
	std::string              title;
	Wt::WDate                date;
	std::optional<Wt::WDate> last_modified;
	std::vector<std::string> tags;
	std::string              slug;
	std::string              filepath;
};
