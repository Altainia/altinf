#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>

#include <memory>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

namespace Wt::Auth
{
	class OAuthService;
	class OAuthProcess;
}

class login_page: public Wt::WContainerWidget
{
public:
	login_page(user_db&                      db,
	           session_data&                 session,
	           const Wt::Auth::OAuthService* google);
	~login_page() override;

	Wt::Signal<> logged_in;

private:
	user_db&                      m_db;
	session_data&                 m_session;
	const Wt::Auth::OAuthService* m_google{nullptr};
	Wt::WLineEdit*                m_username{nullptr};
	Wt::WPasswordEdit*            m_password{nullptr};
	Wt::WText*                    m_error{nullptr};

	// Owned for the page's lifetime so the in-flight OAuth callback stays valid.
	std::unique_ptr<Wt::Auth::OAuthProcess> m_process;

	void submit();
};
