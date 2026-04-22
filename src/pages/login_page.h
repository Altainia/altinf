#pragma once

#include "auth/session_data.h"
#include "auth/user_db.h"

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

#include <functional>

class login_page: public Wt::WContainerWidget
{
public:
	login_page(user_db&              db,
	           session_data&         session,
	           std::function<void()> on_login);

private:
	user_db&              m_db;
	session_data&         m_session;
	std::function<void()> m_on_login;
	Wt::WLineEdit*        m_username{nullptr};
	Wt::WLineEdit*        m_password{nullptr};
	Wt::WText*            m_error{nullptr};

	void submit();
};
