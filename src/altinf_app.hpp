#pragma once

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"
#include "blog/blog_post.hpp"
#include "gantt/gantt.hpp"
#include "gantt/gantt_db.hpp"
#include "links/link.hpp"
#include "links/link_db.hpp"

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

class nav_bar;

class altinf_app: public Wt::WApplication
{
public:
	explicit altinf_app(const Wt::WEnvironment& env);

private:
	session_data                       m_session;
	std::unique_ptr<user_db>           m_user_db;
	std::filesystem::path              m_posts_dir;
	std::vector<blog_post>             m_posts;
	std::unique_ptr<link_db>           m_link_db;
	std::vector<link_entry>            m_links;
	std::optional<link_entry>          m_edit_link;
	std::unique_ptr<gantt_db>          m_gantt_db;
	std::optional<gantt_project_entry> m_edit_project;
	std::optional<user_entry>          m_edit_user;
	nav_bar*                           m_nav{nullptr};
	Wt::WContainerWidget*              m_content{nullptr};

	void handle_path(const std::string& path);
	void reload_posts();
	void reload_links();
};
