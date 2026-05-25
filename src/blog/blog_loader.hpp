#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "blog_post.hpp"

class blog_loader
{
public:
	explicit blog_loader(std::filesystem::path posts_dir);

	std::vector<blog_post> load() const;

private:
	std::filesystem::path m_posts_dir;

	static blog_post   parse_post(const std::filesystem::path& path);
	static std::string derive_slug(const std::filesystem::path& path);
	static std::string trim(const std::string& s);
};