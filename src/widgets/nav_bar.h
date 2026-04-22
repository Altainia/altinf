#pragma once

#include "auth/session_data.h"

#include <Wt/WContainerWidget.h>

class nav_bar: public Wt::WContainerWidget
{
public:
	explicit nav_bar(const session_data& session);

	void update();

private:
	const session_data&   m_session;
	Wt::WContainerWidget* m_auth_area{nullptr};
};
