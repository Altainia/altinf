#include "nav_bar.hpp"

#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "auth/permission.hpp"
#include "paths.hpp"
#include "widgets/notification_bell.hpp"

nav_bar::nav_bar(const session_data& session, org_db* odb):
  m_session{session},
  m_org_db{odb}
{
	setStyleClass("nav-bar");

	// Hamburger toggles the drawer on mobile; hidden on desktop via CSS.
	auto* burger = addNew<Wt::WPushButton>("\xe2\x98\xb0"); // ☰
	burger->setStyleClass("nav-hamburger");
	burger->clicked().connect([this] {
		m_drawer->toggleStyleClass("nav-drawer--open",
		                           !m_drawer->hasStyleClass("nav-drawer--open"));
	});
	m_hamburger = burger;

	auto* brand = addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/"}, "AltInf");
	brand->setStyleClass("nav-brand");

	// Drawer: collapses to inline links on desktop, slides in on mobile.
	m_drawer = addNew<Wt::WContainerWidget>();
	m_drawer->setStyleClass("nav-drawer");

	auto* links = m_drawer->addNew<Wt::WContainerWidget>();
	links->setStyleClass("nav-links");
	drawer_link(links, "/", "Home", "nav-link");
	drawer_link(links, paths::link_list(), "Links", "nav-link");
	drawer_link(links, paths::blog_list(), "Blog", "nav-link");

	m_menu = m_drawer->addNew<Wt::WContainerWidget>();
	m_menu->setStyleClass("nav-menu");

	// Persistent top-bar slots (populated in update()).
	m_org_wrap  = addNew<Wt::WContainerWidget>();
	m_bell_wrap = addNew<Wt::WContainerWidget>();
	m_bell_wrap->setStyleClass("nav-bell-wrap");

	update();
}

Wt::WAnchor* nav_bar::drawer_link(Wt::WContainerWidget* parent,
                                  const std::string&    path,
                                  const std::string&    text,
                                  const std::string&    style)
{
	auto* a = parent->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, path}, text);
	a->setStyleClass(style);
	a->clicked().connect([this] { m_drawer->removeStyleClass("nav-drawer--open"); });
	return a;
}

void nav_bar::update()
{
	m_menu->clear();
	m_org_wrap->clear();
	m_org_wrap->setStyleClass("");
	m_bell_wrap->clear();
	m_bell = nullptr;

	if(!m_session.logged_in)
	{
		drawer_link(m_menu, std::string{paths::login_path}, "Login", "nav-link nav-login");
		return;
	}

	// ── Org selector ──────────────────────────────────────────────────────────
	if(m_org_db)
	{
		const auto orgs = m_org_db->orgs_for_user(m_session.username);
		if(!orgs.empty())
		{
			// Resolve the active org: last-viewed if still valid, else first.
			long long active_id = 0;
			if(const auto last = m_org_db->get_last_org(m_session.username))
			{
				for(const auto& o: orgs)
				{
					if(o.id == *last)
					{
						active_id = o.id;
						break;
					}
				}
			}
			if(active_id == 0)
			{
				active_id = orgs[0].id;
			}

			std::string active_name;
			for(const auto& o: orgs)
			{
				if(o.id == active_id)
				{
					active_name = o.name;
					break;
				}
			}

			m_org_wrap->setStyleClass(
			  orgs.size() > 1 ? "nav-org-area nav-org-area--multi" : "nav-org-area");

			// Primary link — always shown.
			m_org_wrap->addNew<Wt::WAnchor>(
			            Wt::WLink{Wt::LinkType::InternalPath, paths::org_view(active_id)},
			            active_name)
			  ->setStyleClass("nav-link nav-org-link");

			// Caret toggles the dropdown on tap (touch-friendly); desktop also
			// opens it on hover via CSS.
			if(orgs.size() > 1)
			{
				auto* caret = m_org_wrap->addNew<Wt::WText>("\xe2\x96\xbe"); // ▾
				caret->setStyleClass("nav-org-caret");
				caret->clicked().connect([this] {
					m_org_wrap->toggleStyleClass(
					  "nav-org-area--open",
					  !m_org_wrap->hasStyleClass("nav-org-area--open"));
				});

				auto* dropdown = m_org_wrap->addNew<Wt::WContainerWidget>();
				dropdown->setStyleClass("nav-org-dropdown");
				for(const auto& o: orgs)
				{
					auto* item = dropdown->addNew<Wt::WAnchor>(
					  Wt::WLink{Wt::LinkType::InternalPath, paths::org_view(o.id)},
					  o.name);
					item->setStyleClass(
					  o.id == active_id ? "nav-org-item nav-org-item--active" : "nav-org-item");
					item->clicked().connect([this] {
						m_org_wrap->removeStyleClass("nav-org-area--open");
					});
				}
			}
		}
	}

	// ── Admin / author links (drawer) ──────────────────────────────────────────
	if(m_session.permissions.has_any(permission::post_write))
	{
		drawer_link(m_menu, paths::blog_new(), "New Post", "nav-link");
	}

	if(m_session.permissions.has_any(permission::admin) ||
	   m_session.permissions.has_any(permission::manage_users))
	{
		drawer_link(m_menu, paths::account_list(), "Accounts", "nav-link");
	}

	if(m_session.permissions.has_any(permission::org_create) ||
	   m_session.permissions.has_any(permission::admin))
	{
		drawer_link(m_menu, paths::admin_org_list(), "Orgs", "nav-link");
	}

	drawer_link(m_menu, std::string{paths::settings_path}, "Settings", "nav-link");

	// ── Notification bell (top bar) ─────────────────────────────────────────────
	if(m_org_db)
	{
		const int count = m_org_db->unread_count(m_session.username);
		m_bell          = m_bell_wrap->addNew<notification_bell>(count);
	}

	// ── Logout (drawer) ─────────────────────────────────────────────────────────
	drawer_link(m_menu, std::string{paths::logout_path}, "Logout", "nav-link nav-logout");
}

void nav_bar::refresh_bell()
{
	if(m_bell && m_org_db)
	{
		m_bell->set_count(m_org_db->unread_count(m_session.username));
	}
}
