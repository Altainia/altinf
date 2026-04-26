#include "altinf_app.hpp"

#include "auth/permission.hpp"
#include "blog/blog_loader.hpp"
#include "pages/account_editor_page.hpp"
#include "pages/account_manager_page.hpp"
#include "pages/blog_page.hpp"
#include "pages/blog_post_page.hpp"
#include "pages/gantt_editor_page.hpp"
#include "pages/gantt_list_page.hpp"
#include "pages/gantt_view_page.hpp"
#include "pages/link_editor_page.hpp"
#include "pages/links_page.hpp"
#include "pages/login_page.hpp"
#include "pages/main_page.hpp"
#include "pages/post_editor_page.hpp"
#include "widgets/footer.hpp"
#include "widgets/nav_bar.hpp"

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
		{
			throw std::runtime_error{"ALTINF_ADMIN_PASSWORD must be set on first run"};
		}
		const auto all_perms = grant(grant(grant(grant(0ULL, permission::admin),
		                                         permission::post_write),
		                                   permission::gantt_write),
		                             permission::manage_users);
		m_user_db->create_user("admin", pw, all_perms);
	}

	m_posts_dir = app_root / "posts";
	m_posts     = blog_loader{m_posts_dir}.load();

	m_link_db  = std::make_unique<link_db>(db_path);
	m_links    = m_link_db->load_all();
	m_gantt_db = std::make_unique<gantt_db>(db_path);

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

	// root
	if(path == "/" || path.empty())
	{
		m_content->addNew<main_page>();
	}
	// blog
	else if(path == "/blog")
	{
		m_content->addNew<blog_page>(m_posts);
	}
	else if(path.starts_with("/blog/"))
	{
		const auto slug = path.substr(6);
		const auto it   = std::find_if(m_posts.begin(), m_posts.end(), [&slug](const blog_post& p) {
      return p.slug == slug;
    });
		if(it != m_posts.end())
		{
			m_content->addNew<blog_post_page>(*it, m_session);
		}
		else
		{
			m_content->addNew<Wt::WText>("Post not found.", Wt::TextFormat::Plain);
		}
	}
	// post editor
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
	else if(path.starts_with("/admin/edit/"))
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
	// links
	else if(path == "/links")
	{
		auto on_delete = [this](long long id) {
			m_link_db->remove(id);
			reload_links();
			handle_path("/links");
		};
		m_content->addNew<links_page>(m_links, m_session, std::move(on_delete));
	}
	else if(path == "/admin/links/new")
	{
		if(!has_permission(m_session.permissions, permission::post_write))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		m_content->addNew<link_editor_page>(m_link_db.get(), nullptr, [this] {
			reload_links();
			handle_path("/links");
		});
	}
	else if(path.starts_with("/admin/links/edit/"))
	{
		if(!has_permission(m_session.permissions, permission::post_write))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		long long id = 0;
		try
		{
			id = std::stoll(path.substr(18));
		}
		catch(const std::exception&)
		{
			m_content->addNew<Wt::WText>("Invalid link ID.", Wt::TextFormat::Plain);
			return;
		}
		const auto opt = m_link_db->find(id);
		if(!opt)
		{
			m_content->addNew<Wt::WText>("Link not found.", Wt::TextFormat::Plain);
			return;
		}
		m_edit_link = opt;
		m_content->addNew<link_editor_page>(m_link_db.get(), &(*m_edit_link), [this] {
			reload_links();
			handle_path("/links");
		});
	}
	// gantt
	else if(path == "/gantt")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		const auto projects =
		  m_gantt_db->projects_visible_to(m_session.username, m_session.permissions);
		m_content->addNew<gantt_list_page>(projects, m_session);
	}
	else if(path.starts_with("/gantt/") && path.find('/', 7) == std::string::npos)
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		long long id = 0;
		try
		{
			id = std::stoll(path.substr(7));
		}
		catch(const std::exception&)
		{
			m_content->addNew<Wt::WText>("Invalid chart ID.", Wt::TextFormat::Plain);
			return;
		}
		if(!m_gantt_db->can_view(id, m_session.username, m_session.permissions))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		const auto opt = m_gantt_db->find_project(id);
		if(!opt)
		{
			m_content->addNew<Wt::WText>("Chart not found.", Wt::TextFormat::Plain);
			return;
		}
		const auto tasks = m_gantt_db->tasks_for_project(id);
		m_content->addNew<gantt_view_page>(*opt, tasks, m_session, [this, id]() {
			setInternalPath("/admin/gantt/edit/" + std::to_string(id), true);
		});
	}
	else if(path == "/admin/gantt/new")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		if(!has_permission(m_session.permissions, permission::gantt_write) &&
		   !has_permission(m_session.permissions, permission::admin))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		m_content->addNew<gantt_editor_page>(
		  *m_gantt_db, m_session, nullptr, std::vector<gantt_task_entry>{}, std::vector<std::string>{}, [this](long long id) { setInternalPath("/gantt/" + std::to_string(id), true); });
	}
	else if(path.starts_with("/admin/gantt/edit/"))
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		long long id = 0;
		try
		{
			id = std::stoll(path.substr(18));
		}
		catch(const std::exception&)
		{
			m_content->addNew<Wt::WText>("Invalid chart ID.", Wt::TextFormat::Plain);
			return;
		}
		if(!m_gantt_db->can_edit(id, m_session.username, m_session.permissions))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		const auto opt = m_gantt_db->find_project(id);
		if(!opt)
		{
			m_content->addNew<Wt::WText>("Chart not found.", Wt::TextFormat::Plain);
			return;
		}
		m_edit_project     = opt;
		const auto tasks   = m_gantt_db->tasks_for_project(id);
		const auto viewers = m_gantt_db->viewers_for_project(id);
		m_content->addNew<gantt_editor_page>(
		  *m_gantt_db, m_session, &(*m_edit_project), tasks, viewers, [this](long long saved_id) {
			  setInternalPath("/gantt/" + std::to_string(saved_id), true);
		  });
	}
	// accounts
	else if(path == "/admin/accounts")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		if(!has_permission(m_session.permissions, permission::admin) &&
		   !has_permission(m_session.permissions, permission::manage_users))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		auto on_delete = [this](const std::string& username) {
			if(username == m_session.username)
			{
				return;
			}
			m_user_db->delete_user(username);
			handle_path("/admin/accounts");
		};
		m_content->addNew<account_manager_page>(*m_user_db, m_session, std::move(on_delete));
	}
	else if(path == "/admin/accounts/new")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		if(!has_permission(m_session.permissions, permission::admin) &&
		   !has_permission(m_session.permissions, permission::manage_users))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		m_content->addNew<account_editor_page>(m_user_db.get(), nullptr, [this] {
			setInternalPath("/admin/accounts", true);
		});
	}
	else if(path.starts_with("/admin/accounts/edit/"))
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		if(!has_permission(m_session.permissions, permission::admin) &&
		   !has_permission(m_session.permissions, permission::manage_users))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		const auto edit_username = path.substr(21);
		const auto users         = m_user_db->list_users();
		const auto it            = std::find_if(users.begin(), users.end(), [&edit_username](const user_entry& e) {
      return e.username == edit_username;
    });
		if(it == users.end())
		{
			m_content->addNew<Wt::WText>("User not found.", Wt::TextFormat::Plain);
			return;
		}
		m_edit_user = *it;
		m_content->addNew<account_editor_page>(m_user_db.get(), &(*m_edit_user), [this] {
			setInternalPath("/admin/accounts", true);
		});
	}
	// auth
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

void altinf_app::reload_links()
{
	m_links = m_link_db->load_all();
}
