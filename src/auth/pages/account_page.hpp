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

// A logged-in user's personal account page. Currently its job is managing the
// link to a Google account (connect / disconnect) used for faster sign-in.
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

	Wt::WContainerWidget* m_google_section{nullptr};

	void render_google_section();
	void start_connect();
};
