#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

class notifications_page: public Wt::WContainerWidget
{
public:
	notifications_page(org_db& odb, const session_data& session);

	Wt::Signal<> read;

private:
	org_db&               m_db;
	const session_data&   m_session;
	Wt::WContainerWidget* m_list{nullptr};

	void add_dismiss(Wt::WContainerWidget* parent, long long nid);

public:
	void refresh();
};
