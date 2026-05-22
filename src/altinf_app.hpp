#pragma once

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"
#include "blog/blog_post.hpp"
#include "org/kanban.hpp"
#include "org/kanban_db.hpp"
#include "org/team_cap.hpp"
#include "link/link.hpp"
#include "link/link_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

class nav_bar;
class notifications_page;

class altinf_app: public Wt::WApplication
{
public:
	explicit altinf_app(const Wt::WEnvironment& env);
	~altinf_app() override;

private:
	session_data                     m_session;
	std::string                      m_session_token;
	std::unique_ptr<user_db>         m_user_db;
	std::filesystem::path            m_posts_dir;
	std::vector<blog_post>           m_posts;
	std::unique_ptr<link_db>         m_link_db;
	std::vector<link_entry>          m_links;
	std::optional<link_entry>        m_edit_link;
	std::unique_ptr<kanban_db>       m_kanban_db;
	std::unique_ptr<org_db>          m_org_db;
	std::optional<kanban_task_entry> m_edit_task;
	std::optional<user_entry>        m_edit_user;
	nav_bar*                         m_nav{nullptr};
	Wt::WContainerWidget*            m_content{nullptr};
	notifications_page*              m_notifications_page{nullptr};

	void handle_path(const std::string& path);

	// Domain handlers — each receives the path remainder after the prefix
	void handle_blog(std::string_view rem);
	void handle_link(std::string_view rem);
	void handle_org(std::string_view rem);
	void handle_team(std::string_view rem);
	void handle_task(std::string_view rem);
	void handle_admin(std::string_view rem);

	// Fixed-path handlers
	void handle_login();
	void handle_logout();
	void handle_notifications();
	void handle_settings();
	void show_main();

	void reload_posts();
	void reload_links();
	void show_forbidden();
	void show_not_found(const std::string& msg = "Page not found.");
	void set_wide(bool wide);
	void register_with_hub();

	bool            resolve_is_org_lead(long long org_id);
	team_cap::flags resolve_team_caps(long long team_id, long long org_id);
};
