#pragma once

#include <Wt/WAnchor.h>
#include <Wt/WContainerWidget.h>

#include <string>

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

class notification_bell;

class nav_bar: public Wt::WContainerWidget
{
public:
	nav_bar(const session_data& session, org_db* odb);

	void update();
	void refresh_bell();

private:
	const session_data& m_session;
	org_db*             m_org_db{nullptr};

	// Top-bar chrome that stays visible on mobile.
	Wt::WInteractWidget*  m_hamburger{nullptr};
	Wt::WContainerWidget* m_org_wrap{nullptr};
	Wt::WContainerWidget* m_bell_wrap{nullptr};

	// Slide-in drawer (collapses inline on desktop) and its repopulated menu area.
	Wt::WContainerWidget* m_drawer{nullptr};
	Wt::WContainerWidget* m_menu{nullptr};

	notification_bell* m_bell{nullptr};

	// Creates a drawer link that also closes the drawer when tapped.
	Wt::WAnchor* drawer_link(Wt::WContainerWidget* parent,
	                         const std::string&    path,
	                         const std::string&    text,
	                         const std::string&    style);
};