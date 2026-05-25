#pragma once

#include <Wt/WContainerWidget.h>

#include <functional>

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

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

	void add_dismiss(Wt::WContainerWidget* parent, long long nid);

public:
	void refresh();
};