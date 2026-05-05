#include "nav_bar.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "auth/permission.hpp"
#include "widgets/notification_bell.hpp"

nav_bar::nav_bar(const session_data& session, org_db* odb):
  m_session{session},
  m_org_db{odb}
{
	setStyleClass("nav-bar");

	auto* brand = addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/"}, "AltInf");
	brand->setStyleClass("nav-brand");

	auto* links = addNew<Wt::WContainerWidget>();
	links->setStyleClass("nav-links");

	links->addNew<Wt::WAnchor>(
	       Wt::WLink{Wt::LinkType::InternalPath, "/"}, "Home")
	  ->setStyleClass("nav-link");

	links->addNew<Wt::WAnchor>(
	       Wt::WLink{Wt::LinkType::InternalPath, "/links"}, "Links")
	  ->setStyleClass("nav-link");

	links->addNew<Wt::WAnchor>(
	       Wt::WLink{Wt::LinkType::InternalPath, "/blog"}, "Blog")
	  ->setStyleClass("nav-link");

	m_auth_area = addNew<Wt::WContainerWidget>();
	m_auth_area->setStyleClass("nav-auth");

	update();
}

void nav_bar::update()
{
	m_auth_area->clear();

	if(!m_session.logged_in)
	{
		m_auth_area->addNew<Wt::WAnchor>(
		             Wt::WLink{Wt::LinkType::InternalPath, "/login"}, "Login")
		  ->setStyleClass("nav-link nav-login");
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

			auto* org_area = m_auth_area->addNew<Wt::WContainerWidget>();
			org_area->setStyleClass(
			  orgs.size() > 1 ? "nav-org-area nav-org-area--multi" : "nav-org-area");

			// Primary link — always shown.
			org_area->addNew<Wt::WAnchor>(
			          Wt::WLink{Wt::LinkType::InternalPath,
			                    "/org/" + std::to_string(active_id)},
			          active_name)
			  ->setStyleClass("nav-link nav-org-link");

			// Dropdown panel — shown on CSS :hover when there are multiple orgs.
			if(orgs.size() > 1)
			{
				auto* dropdown = org_area->addNew<Wt::WContainerWidget>();
				dropdown->setStyleClass("nav-org-dropdown");
				for(const auto& o: orgs)
				{
					dropdown->addNew<Wt::WAnchor>(
					          Wt::WLink{Wt::LinkType::InternalPath,
					                    "/org/" + std::to_string(o.id)},
					          o.name)
					  ->setStyleClass(o.id == active_id ? "nav-org-item nav-org-item--active" : "nav-org-item");
				}
			}
		}
	}

	// ── Admin / author links ───────────────────────────────────────────────────
	if(m_session.permissions.has_any(permission::post_write))
	{
		m_auth_area->addNew<Wt::WAnchor>(
		             Wt::WLink{Wt::LinkType::InternalPath, "/admin/new"}, "New Post")
		  ->setStyleClass("nav-link");
	}

	if(m_session.permissions.has_any(permission::admin) ||
	   m_session.permissions.has_any(permission::manage_users))
	{
		m_auth_area->addNew<Wt::WAnchor>(
		             Wt::WLink{Wt::LinkType::InternalPath, "/admin/accounts"}, "Accounts")
		  ->setStyleClass("nav-link");
	}

	if(m_session.permissions.has_any(permission::org_create) ||
	   m_session.permissions.has_any(permission::admin))
	{
		m_auth_area->addNew<Wt::WAnchor>(
		             Wt::WLink{Wt::LinkType::InternalPath, "/admin/org"}, "Orgs")
		  ->setStyleClass("nav-link");
	}

	// ── Notification bell ─────────────────────────────────────────────────────
	if(m_org_db)
	{
		const int count = m_org_db->unread_count(m_session.username);
		m_bell          = m_auth_area->addNew<notification_bell>(count);
	}

	// ── Logout ────────────────────────────────────────────────────────────────
	m_auth_area->addNew<Wt::WAnchor>(
	             Wt::WLink{Wt::LinkType::InternalPath, "/logout"}, "Logout")
	  ->setStyleClass("nav-link nav-logout");
}

void nav_bar::refresh_bell()
{
	if(m_bell && m_org_db)
	{
		m_bell->set_count(m_org_db->unread_count(m_session.username));
	}
}
