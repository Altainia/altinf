#include "nav_bar.hpp"

#include "auth/permission.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

nav_bar::nav_bar(const session_data& session):
  m_session{session}
{
	setStyleClass("nav-bar");

	auto* brand = addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/"}, "AltInf");
	brand->setStyleClass("nav-brand");

	auto* links = addNew<Wt::WContainerWidget>();
	links->setStyleClass("nav-links");

	auto* home_link = links->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/"}, "Home");
	home_link->setStyleClass("nav-link");

	auto* links_link = links->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/links"}, "Links");
	links_link->setStyleClass("nav-link");

	auto* blog_link = links->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/blog"}, "Blog");
	blog_link->setStyleClass("nav-link");

	auto* gantt_link = links->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/gantt"}, "Gantt");
	gantt_link->setStyleClass("nav-link");

	m_auth_area = addNew<Wt::WContainerWidget>();
	m_auth_area->setStyleClass("nav-auth");

	update();
}

void nav_bar::update()
{
	m_auth_area->clear();

	if(m_session.logged_in)
	{
		if(has_permission(m_session.permissions, permission::post_write))
		{
			auto* new_post = m_auth_area->addNew<Wt::WAnchor>(
			  Wt::WLink{Wt::LinkType::InternalPath, "/admin/new"}, "New Post");
			new_post->setStyleClass("nav-link");
		}

		if(has_permission(m_session.permissions, permission::admin) ||
		   has_permission(m_session.permissions, permission::manage_users))
		{
			auto* accounts_link = m_auth_area->addNew<Wt::WAnchor>(
			  Wt::WLink{Wt::LinkType::InternalPath, "/admin/accounts"}, "Accounts");
			accounts_link->setStyleClass("nav-link");
		}

		auto* logout_link = m_auth_area->addNew<Wt::WAnchor>(
		  Wt::WLink{Wt::LinkType::InternalPath, "/logout"}, "Logout");
		logout_link->setStyleClass("nav-link nav-logout");
	}
}
