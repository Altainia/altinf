#pragma once

#include <filesystem>
#include <string>

struct post_path
{
	std::filesystem::path filepath;
	std::string           slug;
};

// Converts a title to a URL-safe slug (e.g. "Hello World" -> "hello-world").
std::string make_post_slug(const std::string& title);

// Returns a unique filepath and final slug for a new post in posts_dir,
// appending a counter suffix if the obvious name already exists.
post_path resolve_new_post(const std::filesystem::path& posts_dir,
                           const std::string&           date,
                           const std::string&           title);

// Writes YAML frontmatter + body to filepath.  Returns false on I/O failure.
bool write_post_file(const std::filesystem::path& filepath,
                     const std::string&           title,
                     const std::string&           date,
                     const std::string&           tags,
                     const std::string&           body);
