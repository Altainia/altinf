#pragma once

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

class notification_bell;

class nav_bar: public Wt::WContainerWidget
{
public:
	nav_bar(const session_data& session, org_db* odb);

	void update();
	void refresh_bell();

private:
	const session_data&   m_session;
	org_db*               m_org_db{nullptr};
	Wt::WContainerWidget* m_auth_area{nullptr};
	notification_bell*    m_bell{nullptr};
};
