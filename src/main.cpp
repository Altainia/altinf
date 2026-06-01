#include <Wt/Auth/AuthService.h>
#include <Wt/Auth/GoogleService.h>
#include <Wt/Dbo/Session.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <Wt/WApplication.h>
#include <Wt/WServer.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#include "altinf_app.hpp"
#include "api/post_api_resource.hpp"
#include "auth/user_db.hpp"
#include "db/migrator.hpp"
#include "link/link_db.hpp"
#include "org/kanban_db.hpp"
#include "org/org_db.hpp"

namespace
{
	// Every schema domain, in the order migrations are applied at startup.
	constexpr std::array domains{"auth", "org", "kanban", "link"};

	// Constructing each wrapper runs its domain's migrations against the shared DB
	// file. Front-loading this at startup means a broken migration aborts boot
	// loudly instead of surfacing on the first user request.
	void apply_migrations(const std::string& db_path)
	{
		user_db{db_path};
		org_db{db_path};
		kanban_db{db_path};
		link_db{db_path};
	}

	void print_versions(const std::string& db_path)
	{
		Wt::Dbo::Session session;
		session.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
		for(const char* const domain: domains)
		{
			std::cout << "  " << domain << ": schema v" << db::migrator::current_version(session, domain) << "\n";
		}
	}
} // namespace

int main(int argc, char** argv)
{
	Wt::WServer server{argc, argv};

	const auto db_path   = (std::filesystem::path{server.appRoot()} / "altinf.db").string();
	const auto posts_dir = std::filesystem::path{server.appRoot()} / "posts";

	if(const char* const gen_user = std::getenv("ALTINF_GENERATE_TOKEN"))
	{
		user_db    db{db_path};
		const auto token = db.create_api_token(gen_user);
		std::cout << "API token for '" << gen_user << "': " << token << "\n"
		          << "Store this securely — it will not be shown again.\n";
		return 0;
	}

	if(std::getenv("ALTINF_DB_VERSION"))
	{
		std::cout << "Database schema versions:\n";
		print_versions(db_path);
		return 0;
	}

	if(std::getenv("ALTINF_MIGRATE"))
	{
		try
		{
			apply_migrations(db_path);
		}
		catch(const std::exception& e)
		{
			std::cerr << "Migration failed: " << e.what() << "\n";
			return 1;
		}
		std::cout << "Migrations applied. Database schema versions:\n";
		print_versions(db_path);
		return 0;
	}

	try
	{
		apply_migrations(db_path);
	}
	catch(const std::exception& e)
	{
		std::cerr << "Database migration failed at startup: " << e.what() << "\n";
		return 1;
	}

	auto api = std::make_shared<post_api_resource>(db_path, posts_dir);
	server.addResource(api, "/api/posts");

	// Shared, read-only auth services (server lifetime). Google sign-in is an
	// opt-in feature: only enabled when the google-oauth2-* config properties are
	// present (injected from ALTINF_GOOGLE_* env vars at deploy time).
	const Wt::Auth::AuthService              auth_service;
	std::unique_ptr<Wt::Auth::GoogleService> google_service;
	if(Wt::Auth::GoogleService::configured())
	{
		google_service = std::make_unique<Wt::Auth::GoogleService>(auth_service);
	}
	const Wt::Auth::OAuthService* const google = google_service.get();

	server.addEntryPoint(Wt::EntryPointType::Application,
	                     [google](const Wt::WEnvironment& env) {
		                     return std::make_unique<altinf_app>(env, google);
	                     });

	if(server.start())
	{
		Wt::WServer::waitForShutdown();
		server.stop();
	}

	return 0;
}