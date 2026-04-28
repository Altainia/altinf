#include "altinf_app.hpp"

#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "auth/permission.hpp"
#include "blog/blog_loader.hpp"
#include "pages/account_editor_page.hpp"
#include "pages/account_manager_page.hpp"
#include "pages/blog_page.hpp"
#include "pages/blog_post_page.hpp"
#include "pages/kanban_board_page.hpp"
#include "pages/kanban_task_editor_page.hpp"
#include "pages/kanban_team_page.hpp"
#include "pages/link_editor_page.hpp"
#include "pages/links_page.hpp"
#include "pages/login_page.hpp"
#include "pages/main_page.hpp"
#include "pages/post_editor_page.hpp"
#include "widgets/footer.hpp"
#include "widgets/nav_bar.hpp"

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
		constexpr auto all_perms =
		  permission::admin | permission::post_write | permission::gantt_write |
		  permission::manage_users;
		m_user_db->create_user("admin", pw, all_perms);
	}

	m_posts_dir = app_root / "posts";
	m_posts     = blog_loader{m_posts_dir}.load();

	m_link_db   = std::make_unique<link_db>(db_path);
	m_links     = m_link_db->load_all();
	m_kanban_db = std::make_unique<kanban_db>(db_path);

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

	// Widen the content container for board pages; restore for everything else.
	const bool wide = path.starts_with("/board");
	m_content->setStyleClass(wide ? "site-content site-content--wide" : "site-content");

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
		if(!m_session.permissions.has_any(permission::post_write))
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
		if(!m_session.permissions.has_any(permission::post_write))
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
		if(!m_session.permissions.has_any(permission::post_write))
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
		if(!m_session.permissions.has_any(permission::post_write))
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
	// board (kanban + gantt)
	else if(path == "/board" || path == "/board/gantt")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		m_content->addNew<kanban_board_page>(*m_kanban_db, m_session, path == "/board/gantt");
	}
	else if(path == "/board/task/new")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		const auto teams = m_kanban_db->all_teams();
		if(teams.empty())
		{
			m_content->addNew<Wt::WText>("No team configured.", Wt::TextFormat::Plain);
			return;
		}
		const long long team_id = teams[0].id;
		if(!m_kanban_db->can_edit_board(team_id, m_session.username, m_session.permissions))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		const auto members = m_kanban_db->members_for_team(team_id);
		m_content->addNew<kanban_task_editor_page>(
		  *m_kanban_db, team_id, m_session, nullptr, members, [this] { setInternalPath("/board", true); });
	}
	else if(path.starts_with("/board/task/") && path.ends_with("/edit"))
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		// Extract task id from "/board/task/{id}/edit"
		const auto id_str = path.substr(12, path.size() - 12 - 5);
		long long  id     = 0;
		try
		{
			id = std::stoll(id_str);
		}
		catch(const std::exception&)
		{
			m_content->addNew<Wt::WText>("Invalid task ID.", Wt::TextFormat::Plain);
			return;
		}
		const auto opt = m_kanban_db->find_task(id);
		if(!opt)
		{
			m_content->addNew<Wt::WText>("Task not found.", Wt::TextFormat::Plain);
			return;
		}
		if(!m_kanban_db->can_edit_board(opt->team_id, m_session.username, m_session.permissions))
		{
			m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
			return;
		}
		m_edit_task        = opt;
		const auto members = m_kanban_db->members_for_team(opt->team_id);
		m_content->addNew<kanban_task_editor_page>(
		  *m_kanban_db, opt->team_id, m_session, &(*m_edit_task), members, [this] { setInternalPath("/board", true); });
	}
	else if(path == "/admin/board/team")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		m_content->addNew<kanban_team_page>(*m_kanban_db, m_session);
	}
	// accounts
	else if(path == "/admin/accounts")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		if(!m_session.permissions.has_any(permission::admin) &&
		   !m_session.permissions.has_any(permission::manage_users))
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
		if(!m_session.permissions.has_any(permission::admin) &&
		   !m_session.permissions.has_any(permission::manage_users))
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
		if(!m_session.permissions.has_any(permission::admin) &&
		   !m_session.permissions.has_any(permission::manage_users))
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
