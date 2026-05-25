#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

class login_page: public Wt::WContainerWidget
{
public:
	login_page(user_db& db, session_data& session);

	Wt::Signal<> logged_in;

private:
	user_db&           m_db;
	session_data&      m_session;
	Wt::WLineEdit*     m_username{nullptr};
	Wt::WPasswordEdit* m_password{nullptr};
	Wt::WText*         m_error{nullptr};

	void submit();
};
