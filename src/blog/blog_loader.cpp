#include "blog_loader.hpp"

#include <Wt/WDate.h>

#include <algorithm>
#include <fstream>
#include <sstream>

blog_loader::blog_loader(std::filesystem::path posts_dir):
  m_posts_dir{std::move(posts_dir)}
{
}

std::vector<blog_post> blog_loader::load() const
{
	std::vector<blog_post> posts;

	if(!std::filesystem::exists(m_posts_dir))
	{
		return posts;
	}

	for(const auto& entry: std::filesystem::directory_iterator(m_posts_dir))
	{
		if(entry.path().extension() != ".md")
		{
			continue;
		}
		posts.push_back(parse_post(entry.path()));
	}

	std::sort(posts.begin(), posts.end(), [](const blog_post& a, const blog_post& b) {
		return a.date > b.date;
	});

	return posts;
}

blog_post blog_loader::parse_post(const std::filesystem::path& path)
{
	blog_post post;
	post.filepath = path.string();
	post.slug     = derive_slug(path);

	std::ifstream file{path};
	std::string   line;

	if(!std::getline(file, line) || trim(line) != "---")
	{
		return post;
	}

	while(std::getline(file, line))
	{
		if(trim(line) == "---")
		{
			break;
		}

		const auto colon = line.find(':');
		if(colon == std::string::npos)
		{
			continue;
		}

		const auto key   = trim(line.substr(0, colon));
		const auto value = trim(line.substr(colon + 1));

		if(key == "title")
		{
			post.title = value;
		}
		else if(key == "date")
		{
			post.date = Wt::WDate::fromString(Wt::WString{value}, "yyyy-MM-dd");
		}
		else if(key == "last_modified")
		{
			const auto lm = Wt::WDate::fromString(Wt::WString{value}, "yyyy-MM-dd");
			if(lm.isValid())
			{
				post.last_modified = lm;
			}
		}
		else if(key == "tags")
		{
			std::istringstream tag_stream{value};
			std::string        tag;
			while(std::getline(tag_stream, tag, ','))
			{
				post.tags.push_back(trim(tag));
			}
		}
	}

	return post;
}

std::string blog_loader::derive_slug(const std::filesystem::path& path)
{
	std::string stem = path.stem().string();

	// Strip leading YYYY-MM-DD- date prefix if present
	if(stem.size() > 11 && stem[4] == '-' && stem[7] == '-' && stem[10] == '-')
	{
		stem = stem.substr(11);
	}

	return stem;
}

std::string blog_loader::trim(const std::string& s)
{
	const auto start = s.find_first_not_of(" \t\r\n");
	if(start == std::string::npos)
	{
		return {};
	}
	const auto end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}
