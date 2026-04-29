#pragma once

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>
#include <functional>

class notifications_page: public Wt::WContainerWidget
{
public:
	notifications_page(org_db&               odb,
	                   const session_data&   session,
	                   std::function<void()> on_read = {});

private:
	org_db&               m_db;
	const session_data&   m_session;
	std::function<void()> m_on_read;
	Wt::WContainerWidget* m_list{nullptr};

	void refresh();
};
