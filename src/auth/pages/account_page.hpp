#pragma once

#include <Wt/WContainerWidget.h>

#include <memory>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

namespace Wt::Auth
{
	class OAuthService;
	class OAuthProcess;
}

// A logged-in user's personal account page: change display name and password,
// manage the Google sign-in link (used for faster sign-in), and — with the
// api_token permission — manage personal API tokens. The user must enter their
// current password to change or remove it; password removal and Google
// disconnect are each only offered when another sign-in method remains.
class account_page: public Wt::WContainerWidget
{
public:
	account_page(user_db&                      db,
	             const session_data&           session,
	             const Wt::Auth::OAuthService* google);
	~account_page() override;

private:
	user_db&                      m_db;
	const session_data&           m_session;
	const Wt::Auth::OAuthService* m_google{nullptr};

	// Owned for the page's lifetime so the in-flight OAuth callback stays valid.
	std::unique_ptr<Wt::Auth::OAuthProcess> m_process;

	Wt::WContainerWidget* m_profile_section{nullptr};
	Wt::WContainerWidget* m_password_section{nullptr};
	Wt::WContainerWidget* m_google_section{nullptr};

	void render_profile_section();
	void render_password_section();
	void render_google_section();
	void start_connect();
};
