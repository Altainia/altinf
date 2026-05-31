#include "altinf_app.hpp"

#include <Wt/Http/Cookie.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "admin/account/pages/account_edit_page.hpp"
#include "admin/account/pages/account_list_page.hpp"
#include "auth/pages/login_page.hpp"
#include "auth/permission.hpp"
#include "blog/blog_loader.hpp"
#include "blog/pages/blog_edit_page.hpp"
#include "blog/pages/blog_list_page.hpp"
#include "blog/pages/blog_view_page.hpp"
#include "link/pages/link_edit_page.hpp"
#include "link/pages/link_list_page.hpp"
#include "org/pages/notifications_page.hpp"
#include "org/pages/org_admin_page.hpp"
#include "org/pages/org_board_page.hpp"
#include "org/pages/org_landing_page.hpp"
#include "org/pages/org_management_page.hpp"
#include "org/pages/org_type_manager_page.hpp"
#include "org/pages/settings_page.hpp"
#include "org/pages/task_edit_page.hpp"
#include "org/pages/team_archive_page.hpp"
#include "org/pages/team_kanban_page.hpp"
#include "org/pages/team_settings_page.hpp"
#include "pages/main_page.hpp"
#include "paths.hpp"
#include "widgets/footer.hpp"
#include "widgets/forbidden_widget.hpp"
#include "widgets/live_hub.hpp"
#include "widgets/nav_bar.hpp"
#include "widgets/not_found_widget.hpp"

altinf_app::~altinf_app()
{
	if(m_session.logged_in)
	{
		live_hub::instance().unsubscribe("user:" + m_session.username, sessionId());
	}
}

altinf_app::altinf_app(const Wt::WEnvironment& env):
  Wt::WApplication{env}
{
	setTitle("AltInf");
	enableUpdates(true);
	useStyleSheet(Wt::WLink{"css/altinf.css"});

	const auto app_root = std::filesystem::path{appRoot()};
	const auto db_path  = (app_root / "altinf.db").string();

	m_user_db   = std::make_unique<user_db>(db_path);
	m_kanban_db = std::make_unique<kanban_db>(db_path);
	m_org_db    = std::make_unique<org_db>(db_path);

	if(const auto* raw = env.getCookie("altinf_session"))
	{
		if(m_user_db->verify_session_token(*raw, m_session))
		{
			m_session_token = *raw;
		}
	}

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

	if(m_session.logged_in && !m_session_token.empty())
	{
		register_with_hub();
	}
}

void altinf_app::register_with_hub()
{
	live_hub::instance().subscribe(
	  "user:" + m_session.username,
	  sessionId(),
	  [this] {
		  m_nav->update();
		  if(m_notifications_page)
		  {
			  m_notifications_page->refresh();
		  }
		  triggerUpdate();
	  });
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

team_cap::flags altinf_app::resolve_team_caps(long long team_id, long long org_id)
{
	if(m_session.permissions.has_any(permission::admin))
	{
		return team_cap::admin_caps;
	}
	if(!m_session.logged_in)
	{
		return {};
	}
	if(m_org_db->is_org_lead(org_id, m_session.username))
	{
		return team_cap::org_lead_caps;
	}
	if(m_kanban_db->is_team_lead(team_id, m_session.username))
	{
		return team_cap::team_lead_caps;
	}
	if(m_kanban_db->is_member(team_id, m_session.username))
	{
		return team_cap::team_member_caps;
	}
	if(m_org_db->is_org_member(org_id, m_session.username))
	{
		return team_cap::org_viewer_caps;
	}
	return {};
}

void altinf_app::show_forbidden()
{
	m_content->addNew<forbidden_widget>();
}

void altinf_app::show_not_found(const std::string& msg)
{
	m_content->addNew<not_found_widget>(msg);
}

void altinf_app::handle_path(const std::string& path)
{
	m_content->clear();
	m_notifications_page = nullptr;
	set_wide(false);

	const std::string_view sv{path};

	if(sv == "/" || sv.empty())
	{
		show_main();
		return;
	}
	if(sv == paths::login_path)
	{
		handle_login();
		return;
	}
	if(sv == paths::logout_path)
	{
		handle_logout();
		return;
	}
	if(sv == paths::notifications_path)
	{
		handle_notifications();
		return;
	}
	if(sv == paths::settings_path)
	{
		handle_settings();
		return;
	}

	// Bare domain roots → canonical redirect
	if(sv == "/blog")
	{
		setInternalPath(paths::blog_list(), true);
		return;
	}
	if(sv == "/link")
	{
		setInternalPath(paths::link_list(), true);
		return;
	}
	if(sv == "/admin/account")
	{
		setInternalPath(paths::account_list(), true);
		return;
	}
	if(sv == "/admin/org")
	{
		setInternalPath(paths::admin_org_list(), true);
		return;
	}

	if(sv.starts_with(paths::blog_prefix))
	{
		handle_blog(sv.substr(paths::blog_prefix.size()));
		return;
	}
	if(sv.starts_with(paths::link_prefix))
	{
		handle_link(sv.substr(paths::link_prefix.size()));
		return;
	}
	if(sv.starts_with(paths::org_prefix))
	{
		handle_org(sv.substr(paths::org_prefix.size()));
		return;
	}
	if(sv.starts_with(paths::team_prefix))
	{
		handle_team(sv.substr(paths::team_prefix.size()));
		return;
	}
	if(sv.starts_with(paths::task_prefix))
	{
		handle_task(sv.substr(paths::task_prefix.size()));
		return;
	}
	if(sv.starts_with(paths::admin_prefix))
	{
		handle_admin(sv.substr(paths::admin_prefix.size()));
		return;
	}

	show_not_found();
}

void altinf_app::set_wide(bool wide)
{
	m_content->setStyleClass(
	  wide ? "site-content site-content--wide" : "site-content");
}

void altinf_app::show_main()
{
	m_content->addNew<main_page>();
}

void altinf_app::handle_login()
{
	auto* login = m_content->addNew<login_page>(*m_user_db, m_session);
	login->logged_in.connect([this] {
		try
		{
			const auto raw_token = m_user_db->create_session_token(m_session.username);
			m_session_token      = raw_token;
			Wt::Http::Cookie c{"altinf_session", raw_token};
			c.setHttpOnly(true);
			c.setSecure(true);
			c.setSameSite(Wt::Http::Cookie::SameSite::Strict);
			c.setMaxAge(std::chrono::days{30});
			setCookie(c);
		}
		catch(const std::exception&)
		{
			m_session = session_data{};
			setInternalPath(std::string{paths::login_path}, true);
			return;
		}
		m_nav->update();
		register_with_hub();
		setInternalPath("/", true);
	});
}

void altinf_app::handle_logout()
{
	live_hub::instance().unsubscribe("user:" + m_session.username, sessionId());
	if(!m_session_token.empty())
	{
		m_user_db->delete_session_token(m_session_token);
		removeCookie(Wt::Http::Cookie{"altinf_session"});
		m_session_token.clear();
	}
	m_session = session_data{};
	m_nav->update();
	setInternalPath("/", true);
}

void altinf_app::handle_notifications()
{
	if(!m_session.logged_in)
	{
		setInternalPath(std::string{paths::login_path}, true);
		return;
	}
	m_notifications_page = m_content->addNew<notifications_page>(*m_org_db, m_session);
	m_notifications_page->read.connect([this] { m_nav->refresh_bell(); });
}

void altinf_app::handle_settings()
{
	if(!m_session.logged_in)
	{
		setInternalPath(std::string{paths::login_path}, true);
		return;
	}
	m_content->addNew<settings_page>(*m_org_db, m_session);
}

void altinf_app::handle_blog(std::string_view rem)
{
	const auto seg = paths::take_segment(rem);
	if(seg == paths::list_seg)
	{
		m_content->addNew<blog_list_page>(m_posts);
	}
	else if(seg == paths::view_seg)
	{
		const std::string slug{rem};
		if(slug.empty())
		{
			show_not_found();
			return;
		}
		const auto it = std::ranges::find(m_posts, slug, &blog_post::slug);
		if(it == m_posts.end())
		{
			show_not_found("Post not found.");
			return;
		}
		m_content->addNew<blog_view_page>(*it, m_session);
	}
	else if(seg == paths::edit_seg)
	{
		if(!m_session.permissions.has_any(permission::post_write))
		{
			show_forbidden();
			return;
		}
		const auto it = std::ranges::find(m_posts, rem, &blog_post::slug);
		if(it == m_posts.end())
		{
			show_not_found("Post not found.");
			return;
		}
		auto* p = m_content->addNew<blog_edit_page>(m_posts_dir, &(*it));
		p->saved.connect([this](const std::string& s) {
			reload_posts();
			setInternalPath(paths::blog_view(s), true);
		});
	}
	else if(seg == paths::new_seg)
	{
		if(!m_session.permissions.has_any(permission::post_write))
		{
			show_forbidden();
			return;
		}
		auto* p = m_content->addNew<blog_edit_page>(m_posts_dir, nullptr);
		p->saved.connect([this](const std::string& s) {
			reload_posts();
			setInternalPath(paths::blog_view(s), true);
		});
	}
	else
	{
		show_not_found();
	}
}

void altinf_app::handle_link(std::string_view rem)
{
	const auto seg = paths::take_segment(rem);
	if(seg == paths::list_seg)
	{
		auto* p = m_content->addNew<link_list_page>(m_links, m_session);
		p->deleted.connect([this](long long id) {
			m_link_db->remove(id);
			reload_links();
			handle_path(paths::link_list());
		});
	}
	else if(seg == paths::new_seg)
	{
		if(!m_session.permissions.has_any(permission::post_write))
		{
			show_forbidden();
			return;
		}
		auto* p = m_content->addNew<link_edit_page>(m_link_db.get(), nullptr);
		p->saved.connect([this] {
			reload_links();
			handle_path(paths::link_list());
		});
	}
	else if(seg == paths::edit_seg)
	{
		if(!m_session.permissions.has_any(permission::post_write))
		{
			show_forbidden();
			return;
		}
		const auto id_opt = paths::take_id(rem);
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
		auto* p     = m_content->addNew<link_edit_page>(m_link_db.get(), &(*m_edit_link));
		p->saved.connect([this] {
			reload_links();
			handle_path(paths::link_list());
		});
	}
	else
	{
		show_not_found();
	}
}

void altinf_app::handle_org(std::string_view rem)
{
	if(!m_session.logged_in)
	{
		setInternalPath(std::string{paths::login_path}, true);
		return;
	}

	const auto seg = paths::take_segment(rem);
	if(seg == paths::view_seg)
	{
		const auto org_id_opt = paths::take_id(rem);
		if(!org_id_opt)
		{
			show_not_found();
			return;
		}
		const long long org_id      = *org_id_opt;
		const bool      is_org_lead = resolve_is_org_lead(org_id);

		if(!is_org_lead && !m_org_db->is_org_member(org_id, m_session.username))
		{
			show_forbidden();
			return;
		}

		const auto sub = paths::take_segment(rem);
		if(sub.empty())
		{
			m_org_db->set_last_org(m_session.username, org_id);
			m_content->addNew<org_landing_page>(
			  *m_org_db, *m_kanban_db, org_id, m_session, is_org_lead);
		}
		else if(sub == "board")
		{
			if(!is_org_lead)
			{
				show_forbidden();
				return;
			}
			set_wide(true);
			m_org_db->set_last_org(m_session.username, org_id);
			m_content->addNew<org_board_page>(
			  *m_org_db, *m_kanban_db, org_id, m_session);
		}
		else if(sub == "types")
		{
			if(!is_org_lead)
			{
				show_forbidden();
				return;
			}
			m_content->addNew<org_type_manager_page>(
			  *m_kanban_db, *m_org_db, org_id, m_session);
		}
		else
		{
			show_not_found();
		}
	}
	else if(seg == paths::edit_seg)
	{
		const auto org_id_opt = paths::take_id(rem);
		if(!org_id_opt)
		{
			show_not_found();
			return;
		}
		const long long org_id      = *org_id_opt;
		const bool      is_org_lead = resolve_is_org_lead(org_id);
		if(!is_org_lead)
		{
			show_forbidden();
			return;
		}
		m_content->addNew<org_management_page>(
		  *m_org_db, *m_kanban_db, *m_user_db, org_id, m_session, paths::org_view(org_id));
	}
	else
	{
		show_not_found();
	}
}

void altinf_app::handle_team(std::string_view rem)
{
	if(!m_session.logged_in)
	{
		setInternalPath(std::string{paths::login_path}, true);
		return;
	}

	const auto seg = paths::take_segment(rem);
	if(seg == paths::view_seg)
	{
		const auto team_id_opt = paths::take_id(rem);
		if(!team_id_opt)
		{
			show_not_found();
			return;
		}
		const long long team_id = *team_id_opt;
		const auto      team    = m_kanban_db->find_team(team_id);
		if(!team)
		{
			show_not_found("Team not found.");
			return;
		}

		const auto caps     = resolve_team_caps(team_id, team->org_id);
		const auto settings = m_kanban_db->settings_for_team(team_id);
		const auto sub      = paths::take_segment(rem);

		if(sub.empty())
		{
			setInternalPath(paths::team_kanban(team_id), true);
		}
		else if(sub == "kanban")
		{
			if(!caps.has_any(team_cap::view_board))
			{
				show_forbidden();
				return;
			}
			set_wide(true);
			m_content->addNew<team_kanban_page>(
			  *m_kanban_db, *m_org_db, m_session, team_id, caps, settings, false);
		}
		else if(sub == "gantt")
		{
			if(!caps.has_any(team_cap::view_board))
			{
				show_forbidden();
				return;
			}
			set_wide(true);
			m_content->addNew<team_kanban_page>(
			  *m_kanban_db, *m_org_db, m_session, team_id, caps, settings, true);
		}
		else if(sub == "task")
		{
			const auto task_sub = paths::take_segment(rem);
			if(task_sub == paths::new_seg)
			{
				if(!caps.has_any(team_cap::create_task))
				{
					show_forbidden();
					return;
				}
				{
					auto* page = m_content->addNew<task_edit_page>(
					  *m_kanban_db, *m_org_db, team_id, m_session, caps, settings, nullptr);
					page->saved.connect([this, team_id] {
						setInternalPath(paths::team_kanban(team_id), true);
					});
				}
			}
			else
			{
				show_not_found();
			}
		}
		else if(sub == "archive")
		{
			if(!caps.has_any(team_cap::view_archived))
			{
				show_forbidden();
				return;
			}
			m_content->addNew<team_archive_page>(*m_kanban_db, m_session, team_id);
		}
		else
		{
			show_not_found();
		}
	}
	else if(seg == paths::edit_seg)
	{
		const auto team_id_opt = paths::take_id(rem);
		if(!team_id_opt)
		{
			show_not_found();
			return;
		}
		const long long team_id = *team_id_opt;
		const auto      team    = m_kanban_db->find_team(team_id);
		if(!team)
		{
			show_not_found("Team not found.");
			return;
		}

		const auto caps = resolve_team_caps(team_id, team->org_id);
		if(!caps.has_any(team_cap::manage_team))
		{
			show_forbidden();
			return;
		}

		const auto sub = paths::take_segment(rem);
		if(sub.empty())
		{
			setInternalPath(paths::team_edit_members(team_id), true);
		}
		else if(sub == "settings")
		{
			m_content->addNew<team_settings_page>(*m_kanban_db, m_session, team_id);
		}
		else
		{
			show_not_found();
		}
	}
	else
	{
		show_not_found();
	}
}

void altinf_app::handle_task(std::string_view rem)
{
	if(!m_session.logged_in)
	{
		setInternalPath(std::string{paths::login_path}, true);
		return;
	}

	const auto seg = paths::take_segment(rem);
	if(seg == paths::edit_seg)
	{
		const auto task_id_opt = paths::take_id(rem);
		if(!task_id_opt)
		{
			show_not_found();
			return;
		}
		const auto opt = m_kanban_db->find_task(*task_id_opt);
		if(!opt)
		{
			show_not_found("Task not found.");
			return;
		}

		// team_id is derived from the task; no team cross-check needed (unlike the old board URL).
		const long long team_id = opt->team_id;
		const auto      team    = m_kanban_db->find_team(team_id);
		if(!team)
		{
			show_not_found("Team not found.");
			return;
		}

		const auto caps     = resolve_team_caps(team_id, team->org_id);
		const auto settings = m_kanban_db->settings_for_team(team_id);

		if(!caps.has_any(team_cap::view_board))
		{
			show_forbidden();
			return;
		}
		if(opt->is_archived && !caps.has_any(team_cap::view_archived))
		{
			show_not_found("Task not found.");
			return;
		}

		m_edit_task = opt;
		{
			auto* page = m_content->addNew<task_edit_page>(
			  *m_kanban_db, *m_org_db, team_id, m_session, caps, settings, &(*m_edit_task));
			page->saved.connect([this, team_id] {
				setInternalPath(paths::team_kanban(team_id), true);
			});
		}
	}
	else
	{
		show_not_found();
	}
}

void altinf_app::handle_admin(std::string_view rem)
{
	if(!m_session.logged_in)
	{
		setInternalPath(std::string{paths::login_path}, true);
		return;
	}

	const auto domain = paths::take_segment(rem);
	if(domain == "account")
	{
		if(!m_session.permissions.has_any(permission::admin | permission::manage_users))
		{
			show_forbidden();
			return;
		}
		const auto seg = paths::take_segment(rem);
		if(seg == paths::list_seg)
		{
			auto* p = m_content->addNew<account_list_page>(*m_user_db, m_session);
			p->deleted.connect([this](const std::string& username) {
				if(username == m_session.username)
				{
					return;
				}
				const auto del_orgs     = m_org_db->orgs_for_user(username);
				const auto del_team_ids = m_kanban_db->team_ids_for_user(username);
				m_user_db->delete_user(username);
				m_org_db->remove_user_from_all_orgs(username);
				m_kanban_db->remove_member_from_all_teams(username);
				live_hub::instance().broadcast("accounts");
				for(const auto& org: del_orgs)
				{
					live_hub::instance().broadcast("org:" + std::to_string(org.id));
				}
				for(const auto tid: del_team_ids)
				{
					live_hub::instance().broadcast("team:" + std::to_string(tid));
				}
				handle_path(paths::account_list());
			});
		}
		else if(seg == paths::new_seg)
		{
			auto* p = m_content->addNew<account_edit_page>(m_user_db.get(), nullptr);
			p->saved.connect([this] { setInternalPath(paths::account_list(), true); });
		}
		else if(seg == paths::edit_seg)
		{
			const auto users = m_user_db->list_users();
			const auto it    = std::ranges::find(users, rem, &user_entry::username);
			if(it == users.end())
			{
				show_not_found("User not found.");
				return;
			}
			m_edit_user = *it;
			auto* p     = m_content->addNew<account_edit_page>(m_user_db.get(), &(*m_edit_user));
			p->saved.connect([this] { setInternalPath(paths::account_list(), true); });
		}
		else
		{
			show_not_found();
		}
	}
	else if(domain == "org")
	{
		if(!m_session.permissions.has_any(permission::org_create | permission::admin))
		{
			show_forbidden();
			return;
		}
		const auto seg = paths::take_segment(rem);
		if(seg == paths::list_seg)
		{
			m_content->addNew<org_admin_page>(*m_org_db, m_session);
		}
		else
		{
			show_not_found();
		}
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