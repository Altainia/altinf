#include "post_writer.hpp"

#include <Wt/WDate.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

std::string make_post_slug(const std::string& title)
{
	std::string slug;
	for(const char c: title)
	{
		if(std::isalnum(static_cast<unsigned char>(c)))
		{
			slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		else if(std::isspace(static_cast<unsigned char>(c)) && !slug.empty() && slug.back() != '-')
		{
			slug += '-';
		}
	}
	while(!slug.empty() && slug.back() == '-')
	{
		slug.pop_back();
	}
	return slug.empty() ? "post" : slug;
}

post_path resolve_new_post(const std::filesystem::path& posts_dir, const std::string& title)
{
	const std::string date_str = Wt::WDate::currentDate().toString("yyyy-MM-dd").toUTF8();
	std::string       slug     = make_post_slug(title);
	std::string       base     = date_str + "-" + slug;
	auto              filepath = posts_dir / (base + ".md");

	if(std::filesystem::exists(filepath))
	{
		for(int n = 2;; ++n)
		{
			const auto candidate = posts_dir / (base + "-" + std::to_string(n) + ".md");
			if(!std::filesystem::exists(candidate))
			{
				filepath = candidate;
				slug     = slug + "-" + std::to_string(n);
				break;
			}
		}
	}

	return {filepath, slug};
}

bool write_post_file(const std::filesystem::path& filepath,
                     const std::string&           title,
                     Wt::WDate                    date,
                     std::optional<Wt::WDate>     last_modified,
                     const std::string&           tags,
                     const std::string&           body)
{
	std::ofstream out{filepath};
	if(!out)
	{
		return false;
	}

	out << "---\n";
	out << "title: " << title << "\n";
	out << "date: " << date.toString("yyyy-MM-dd").toUTF8() << "\n";
	if(last_modified)
	{
		out << "last_modified: " << last_modified->toString("yyyy-MM-dd").toUTF8() << "\n";
	}
	out << "tags: " << tags << "\n";
	out << "---\n";
	out << body;
	out.flush();
	return true;
}
