#pragma once

#include <Wt/WDate.h>

#include <filesystem>
#include <optional>
#include <string>

struct post_path
{
	std::filesystem::path filepath;
	std::string           slug;
};

// Converts a title to a URL-safe slug (e.g. "Hello World" -> "hello-world").
std::string make_post_slug(const std::string& title);

// Returns a unique filepath and final slug for a new post in posts_dir,
// using today's date as the filename prefix.  Appends a counter suffix if
// the obvious name already exists.
post_path resolve_new_post(const std::filesystem::path& posts_dir, const std::string& title);

// Writes YAML frontmatter + body to filepath.  Returns false on I/O failure.
// last_modified is written only when it has a value.
bool write_post_file(const std::filesystem::path&     filepath,
                     const std::string&               title,
                     Wt::WDate                        date,
                     std::optional<Wt::WDate>         last_modified,
                     const std::string&               tags,
                     const std::string&               body);
