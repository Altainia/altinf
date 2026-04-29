#pragma once

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"
#include "blog/blog_post.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"
#include "links/link.hpp"
#include "links/link_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>

#include <filesystem>
#include <memory>
#include <optional>
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
	void reload_posts();
	void reload_links();
	void show_forbidden();
	void show_not_found(const std::string& msg = "Page not found.");
	void register_with_hub();

	// Compute lead status for a team, given a pre-resolved org lead flag.
	bool resolve_is_org_lead(long long org_id);
};
