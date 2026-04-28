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
#include "pages/notifications_page.hpp"
#include "pages/org_admin_page.hpp"
#include "pages/org_board_page.hpp"
#include "pages/org_landing_page.hpp"
#include "pages/post_editor_page.hpp"
#include "widgets/footer.hpp"
#include "widgets/nav_bar.hpp"

// Parse a decimal integer from path[offset] up to the next '/' or end.
static std::optional<long long> parse_id(const std::string& path, size_t offset)
{
	const auto end = path.find('/', offset);
	const auto seg = (end == std::string::npos) ? path.substr(offset) : path.substr(offset, end - offset);
	if(seg.empty())
	{
		return std::nullopt;
	}
	try
	{
		return std::stoll(seg);
	}
	catch(...)
	{
		return std::nullopt;
	}
}

// Return the portion of path that follows the first numeric segment after offset.
static std::string suffix_after_id(const std::string& path, size_t offset)
{
	const auto slash = path.find('/', offset);
	return (slash == std::string::npos) ? "" : path.substr(slash);
}

altinf_app::altinf_app(const Wt::WEnvironment& env):
  Wt::WApplication{env}
{
	setTitle("AltInf");
	useStyleSheet(Wt::WLink{"css/altinf.css"});

	const auto app_root = std::filesystem::path{appRoot()};
	const auto db_path  = (app_root / "altinf.db").string();

	m_user_db   = std::make_unique<user_db>(db_path);
	m_kanban_db = std::make_unique<kanban_db>(db_path);
	m_org_db    = std::make_unique<org_db>(db_path);

	if(!m_user_db->has_users())
	{
		const char* const pw = std::getenv("ALTINF_ADMIN_PASSWORD");
		if(!pw)
		{
			throw std::runtime_error{"ALTINF_ADMIN_PASSWORD must be set on first run"};
		}

		constexpr auto all_perms = permission::admin | permission::post_write |
		                           permission::org_create | permission::manage_users;
		m_user_db->create_user("admin", pw, all_perms);
	}

	m_posts_dir = app_root / "posts";
	m_posts     = blog_loader{m_posts_dir}.load();
	m_link_db   = std::make_unique<link_db>(db_path);
	m_links     = m_link_db->load_all();

	root()->setStyleClass("site-root");
	m_nav     = root()->addNew<nav_bar>(m_session, m_org_db.get());
	m_content = root()->addNew<Wt::WContainerWidget>();
	m_content->setStyleClass("site-content");
	root()->addNew<footer>();

	internalPathChanged().connect([this](const std::string& path) {
		handle_path(path);
	});
	handle_path(internalPath());
}

bool altinf_app::resolve_is_org_lead(long long org_id)
{
	if(m_session.permissions.has_any(permission::admin))
	{
		return true;
	}
	if(org_id == 0)
	{
		return false;
	}
	return m_org_db->is_org_lead(org_id, m_session.username);
}

void altinf_app::show_forbidden()
{
	m_content->addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
}

void altinf_app::show_not_found(const std::string& msg)
{
	m_content->addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
}

void altinf_app::handle_path(const std::string& path)
{
	m_content->clear();

	const bool wide = path.starts_with("/board/") ||
	                  (path.starts_with("/org/") && path.find("/board") != std::string::npos);
	m_content->setStyleClass(wide ? "site-content site-content--wide" : "site-content");

	// ── Root ──────────────────────────────────────────────────────────────────
	if(path == "/" || path.empty())
	{
		m_content->addNew<main_page>();
	}
	// ── Blog ──────────────────────────────────────────────────────────────────
	else if(path == "/blog")
	{
		m_content->addNew<blog_page>(m_posts);
	}
	else if(path.starts_with("/blog/"))
	{
		const auto slug = path.substr(6);
		const auto it   = std::find_if(
      m_posts.begin(), m_posts.end(), [&slug](const blog_post& p) { return p.slug == slug; });
		if(it != m_posts.end())
		{
			m_content->addNew<blog_post_page>(*it, m_session);
		}
		else
		{
			show_not_found("Post not found.");
		}
	}
	// ── Post editor ───────────────────────────────────────────────────────────
	else if(path == "/admin/new")
	{
		if(!m_session.permissions.has_any(permission::post_write))
		{
			show_forbidden();
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
			show_forbidden();
			return;
		}
		const auto slug = path.substr(12);
		const auto it   = std::find_if(
      m_posts.begin(), m_posts.end(), [&slug](const blog_post& p) { return p.slug == slug; });
		if(it == m_posts.end())
		{
			show_not_found("Post not found.");
			return;
		}
		m_content->addNew<post_editor_page>(
		  m_posts_dir, &(*it), [this](const std::string& s) {
			  reload_posts();
			  setInternalPath("/blog/" + s, true);
		  });
	}
	// ── Links ─────────────────────────────────────────────────────────────────
	else if(path == "/links")
	{
		m_content->addNew<links_page>(m_links, m_session, [this](long long id) {
			m_link_db->remove(id);
			reload_links();
			handle_path("/links");
		});
	}
	else if(path == "/admin/links/new")
	{
		if(!m_session.permissions.has_any(permission::post_write))
		{
			show_forbidden();
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
			show_forbidden();
			return;
		}
		const auto id_opt = parse_id(path, 18);
		if(!id_opt)
		{
			show_not_found("Invalid link ID.");
			return;
		}
		const auto opt = m_link_db->find(*id_opt);
		if(!opt)
		{
			show_not_found("Link not found.");
			return;
		}
		m_edit_link = opt;
		m_content->addNew<link_editor_page>(m_link_db.get(), &(*m_edit_link), [this] {
			reload_links();
			handle_path("/links");
		});
	}
	// ── Team board (/board/{team_id}[/gantt|/task/new|/task/{id}/edit]) ───────
	else if(path.starts_with("/board/"))
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}

		const auto team_id_opt = parse_id(path, 7);
		if(!team_id_opt)
		{
			show_not_found();
			return;
		}
		const long long team_id = *team_id_opt;

		const auto team = m_kanban_db->find_team(team_id);
		if(!team)
		{
			show_not_found("Team not found.");
			return;
		}

		const bool is_org_lead  = resolve_is_org_lead(team->org_id);
		const bool is_team_lead = m_kanban_db->is_team_lead(team_id, m_session.username);
		const bool is_lead      = is_org_lead || is_team_lead;

		const std::string suffix = suffix_after_id(path, 7);

		if(suffix.empty() || suffix == "/")
		{
			if(!m_kanban_db->can_view_board(
			     team_id, m_session.username, m_session.permissions, is_org_lead))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<kanban_board_page>(
			  *m_kanban_db, m_session, team_id, is_lead, false);
		}
		else if(suffix == "/gantt")
		{
			if(!m_kanban_db->can_view_board(
			     team_id, m_session.username, m_session.permissions, is_org_lead))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<kanban_board_page>(
			  *m_kanban_db, m_session, team_id, is_lead, true);
		}
		else if(suffix == "/task/new")
		{
			if(!is_lead)
			{
				show_forbidden();
				return;
			}
			const auto members = m_kanban_db->members_for_team(team_id);
			m_content->addNew<kanban_task_editor_page>(
			  *m_kanban_db, *m_org_db, team_id, m_session, is_lead, nullptr, members, [this, team_id] {
				  setInternalPath("/board/" + std::to_string(team_id), true);
			  });
		}
		else if(suffix.starts_with("/task/") && suffix.ends_with("/edit"))
		{
			const auto task_id_opt = parse_id(suffix, 6);
			if(!task_id_opt)
			{
				show_not_found();
				return;
			}
			const auto opt = m_kanban_db->find_task(*task_id_opt);
			if(!opt || opt->team_id != team_id)
			{
				show_not_found("Task not found.");
				return;
			}
			if(!m_kanban_db->can_view_board(
			     team_id, m_session.username, m_session.permissions, is_org_lead))
			{
				show_forbidden();
				return;
			}
			m_edit_task        = opt;
			const auto members = m_kanban_db->members_for_team(team_id);
			m_content->addNew<kanban_task_editor_page>(
			  *m_kanban_db, *m_org_db, team_id, m_session, is_lead, &(*m_edit_task), members, [this, team_id] {
				  setInternalPath("/board/" + std::to_string(team_id), true);
			  });
		}
		else
		{
			show_not_found();
		}
	}
	// ── Organisation routes (/org/{org_id}[/board|/manage]) ──────────────────
	else if(path.starts_with("/org/"))
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}

		const auto org_id_opt = parse_id(path, 5);
		if(!org_id_opt)
		{
			show_not_found();
			return;
		}
		const long long org_id = *org_id_opt;

		const bool is_org_lead = resolve_is_org_lead(org_id);

		if(!is_org_lead && !m_org_db->is_org_member(org_id, m_session.username))
		{
			show_forbidden();
			return;
		}

		const std::string suffix = suffix_after_id(path, 5);

		if(suffix.empty() || suffix == "/")
		{
			m_org_db->set_last_org(m_session.username, org_id);
			m_content->addNew<org_landing_page>(
			  *m_org_db, *m_kanban_db, org_id, m_session, is_org_lead);
		}
		else if(suffix == "/board")
		{
			if(!is_org_lead)
			{
				show_forbidden();
				return;
			}
			m_org_db->set_last_org(m_session.username, org_id);
			m_content->addNew<org_board_page>(
			  *m_org_db, *m_kanban_db, org_id, m_session);
		}
		else if(suffix == "/manage")
		{
			if(!is_org_lead)
			{
				show_forbidden();
				return;
			}
			m_content->addNew<kanban_team_page>(
			  *m_org_db, *m_kanban_db, org_id, m_session);
		}
		else
		{
			show_not_found();
		}
	}
	// ── Org admin (create orgs) ───────────────────────────────────────────────
	else if(path == "/admin/org")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		if(!m_session.permissions.has_any(permission::org_create) &&
		   !m_session.permissions.has_any(permission::admin))
		{
			show_forbidden();
			return;
		}
		m_content->addNew<org_admin_page>(*m_org_db, m_session);
	}
	// ── Notifications ─────────────────────────────────────────────────────────
	else if(path == "/notifications")
	{
		if(!m_session.logged_in)
		{
			setInternalPath("/login", true);
			return;
		}
		m_content->addNew<notifications_page>(*m_org_db, m_session);
	}
	// ── Accounts ──────────────────────────────────────────────────────────────
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
			show_forbidden();
			return;
		}
		m_content->addNew<account_manager_page>(
		  *m_user_db, m_session, [this](const std::string& username) {
			  if(username == m_session.username)
			  {
				  return;
			  }
			  m_user_db->delete_user(username);
			  handle_path("/admin/accounts");
		  });
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
			show_forbidden();
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
			show_forbidden();
			return;
		}
		const auto edit_username = path.substr(21);
		const auto users         = m_user_db->list_users();
		const auto it            = std::find_if(
      users.begin(), users.end(), [&edit_username](const user_entry& e) { return e.username == edit_username; });
		if(it == users.end())
		{
			show_not_found("User not found.");
			return;
		}
		m_edit_user = *it;
		m_content->addNew<account_editor_page>(
		  m_user_db.get(), &(*m_edit_user), [this] { setInternalPath("/admin/accounts", true); });
	}
	// ── Auth ──────────────────────────────────────────────────────────────────
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
		show_not_found();
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
