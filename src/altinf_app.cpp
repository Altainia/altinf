#include "altinf_app.h"

#include "auth/permission.h"
#include "blog/blog_loader.h"
#include "pages/blog_page.h"
#include "pages/blog_post_page.h"
#include "pages/login_page.h"
#include "pages/main_page.h"
#include "pages/post_editor_page.h"
#include "widgets/footer.h"
#include "widgets/nav_bar.h"

#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

altinf_app::altinf_app(const Wt::WEnvironment& env):
  Wt::WApplication{env}
{
	setTitle("AltInf");
	useStyleSheet(Wt::WLink{"css/altinf.css"});

	const auto app_root = std::filesystem::path{appRoot()};

	const auto db_path = (app_root / "altinf.db").string();
	m_user_db          = std::make_unique<user_db>(db_path);

	if(!m_user_db->has_users())
	{
		const char* const pw = std::getenv("ALTINF_ADMIN_PASSWORD");
		if(!pw)
			throw std::runtime_error{"ALTINF_ADMIN_PASSWORD must be set on first run"};
		const auto all_perms = grant(grant(0ULL, permission::admin), permission::post_write);
		m_user_db->create_user("admin", pw, all_perms);
	}

	m_posts_dir = app_root / "posts";
	m_posts     = blog_loader{m_posts_dir}.load();

	root()->setStyleClass("site-root");
	m_nav     = root()->addNew<nav_bar>(m_session);
	m_content = root()->addNew<Wt::WContainerWidget>();
	m_content->setStyleClass("site-content");
	root()->addNew<footer>();

	internalPathChanged().connect([this](const std::string& path) {
		handle_path(path);
	});

	handle_path(internalPath());
}

void altinf_app::handle_path(const std::string& path)
{
	m_content->clear();

	if(path == "/" || path.empty())
	{
		m_content->addNew<main_page>();
	}
	else if(path == "/blog")
	{
		m_content->addNew<blog_page>(m_posts);
	}
	else if(path.substr(0, 6) == "/blog/")
	{
		const auto slug = path.substr(6);
		const auto it   = std::find_if(m_posts.begin(), m_posts.end(), [&slug](const blog_post& p) {
			return p.slug == slug;
		});

		if(it != m_posts.end())
			m_content->addNew<blog_post_page>(*it, m_session);
		else
			m_content->addNew<Wt::WText>("Post not found.", Wt::TextFormat::Plain);
	}
	else if(path == "/admin/new")
	{
		if(!has_permission(m_session.permissions, permission::post_write))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		m_content->addNew<post_editor_page>(m_posts_dir, nullptr, [this](const std::string& slug) {
			reload_posts();
			setInternalPath("/blog/" + slug, true);
		});
	}
	else if(path.size() > 12 && path.substr(0, 12) == "/admin/edit/")
	{
		if(!has_permission(m_session.permissions, permission::post_write))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		const auto slug = path.substr(12);
		const auto it   = std::find_if(m_posts.begin(), m_posts.end(), [&slug](const blog_post& p) {
			return p.slug == slug;
		});
		if(it == m_posts.end())
		{
			m_content->addNew<Wt::WText>("Post not found.", Wt::TextFormat::Plain);
			return;
		}
		m_content->addNew<post_editor_page>(m_posts_dir, &(*it), [this](const std::string& s) {
			reload_posts();
			setInternalPath("/blog/" + s, true);
		});
	}
	else if(path == "/login")
	{
		m_content->addNew<login_page>(*m_user_db, m_session, [this] {
			m_nav->update();
			setInternalPath("/", true);
		});
	}
	else if(path == "/logout")
	{
		m_session = session_data{};
		m_nav->update();
		setInternalPath("/", true);
	}
	else
	{
		m_content->addNew<Wt::WText>("Page not found.", Wt::TextFormat::Plain);
	}
}

void altinf_app::reload_posts()
{
	m_posts = blog_loader{m_posts_dir}.load();
}
