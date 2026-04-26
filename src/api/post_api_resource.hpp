#pragma once

#include <Wt/WResource.h>

#include <filesystem>
#include <string>

class post_api_resource: public Wt::WResource
{
public:
	post_api_resource(std::string db_path, std::filesystem::path posts_dir);
	~post_api_resource() override;

private:
	void handleRequest(const Wt::Http::Request& request,
	                   Wt::Http::Response&      response) override;

	std::string           m_db_path;
	std::filesystem::path m_posts_dir;
};
