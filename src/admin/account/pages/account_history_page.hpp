#pragma once

#include <Wt/WContainerWidget.h>

#include <string>

#include "auth/user_db.hpp"

// Read-only audit history for a single user account, gated by the
// view_user_history permission at the route. Renders events newest-first as a
// timeline, mirroring the task history view.
class account_history_page: public Wt::WContainerWidget
{
public:
	account_history_page(user_db& db, std::string username);

private:
	user_db&    m_db;
	std::string m_username;
};
