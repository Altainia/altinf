#include "altinf_app.h"
#include "api/post_api_resource.h"

#include <Wt/WApplication.h>
#include <Wt/WServer.h>

#include <filesystem>
#include <memory>

int main(int argc, char** argv)
{
	Wt::WServer server{argc, argv};

	const auto db_path   = (std::filesystem::path{server.appRoot()} / "altinf.db").string();
	const auto posts_dir = std::filesystem::path{server.appRoot()} / "posts";

	auto api = std::make_shared<post_api_resource>(db_path, posts_dir);
	server.addResource(api, "/api/posts");

	server.addEntryPoint(Wt::EntryPointType::Application,
	                     [](const Wt::WEnvironment& env) {
		                     return std::make_unique<altinf_app>(env);
	                     });

	if(server.start())
	{
		Wt::WServer::waitForShutdown();
		server.stop();
	}

	return 0;
}
