#pragma once

#include "auth/session_data.h"
#include "auth/user_db.h"
#include "blog/blog_post.h"

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>

#include <filesystem>
#include <memory>
#include <vector>

class nav_bar;

class altinf_app: public Wt::WApplication
{
public:
	explicit altinf_app(const Wt::WEnvironment& env);

private:
	session_data             m_session;
	std::unique_ptr<user_db> m_user_db;
	std::filesystem::path    m_posts_dir;
	std::vector<blog_post>   m_posts;
	nav_bar*                 m_nav{nullptr};
	Wt::WContainerWidget*    m_content{nullptr};

	void handle_path(const std::string& path);
	void reload_posts();
};
