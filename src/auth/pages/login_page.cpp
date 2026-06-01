#include "login_page.hpp"

#include <Wt/Auth/Identity.h>
#include <Wt/Auth/OAuthService.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

// Defined here, where Wt::Auth::OAuthService/OAuthProcess are complete types, so
// the m_process unique_ptr can be destroyed.
login_page::~login_page() = default;

login_page::login_page(user_db&                      db,
                       session_data&                 session,
                       const Wt::Auth::OAuthService* google):
  m_db{db},
  m_session{session},
  m_google{google}
{
	setStyleClass("page login-page");

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("login-form");

	form->addNew<Wt::WText>("<h2>Login</h2>", Wt::TextFormat::UnsafeXHTML);

	m_username = form->addNew<Wt::WLineEdit>();
	m_username->setPlaceholderText("Username");
	m_username->setStyleClass("login-field");
	m_username->setFocus();

	m_password = form->addNew<Wt::WPasswordEdit>();
	m_password->setPlaceholderText("Password");
	m_password->setStyleClass("login-field");

	m_error = form->addNew<Wt::WText>();
	m_error->setStyleClass("login-error");

	auto* submit_btn = form->addNew<Wt::WPushButton>("Login");
	submit_btn->setStyleClass("login-btn");
	submit_btn->clicked().connect(this, &login_page::submit);

	m_username->enterPressed().connect(this, &login_page::submit);
	m_password->enterPressed().connect(this, &login_page::submit);

	if(m_google != nullptr)
	{
		m_process = m_google->createProcess(m_google->authenticationScope());

		auto* google_btn =
		  form->addNew<Wt::WPushButton>("Sign in with Google");
		google_btn->setStyleClass("login-btn login-google-btn");
		google_btn->clicked().connect(m_process.get(),
		                              &Wt::Auth::OAuthProcess::startAuthenticate);

		m_process->authenticated().connect(
		  [this](const Wt::Auth::Identity& identity) {
			  if(!identity.isValid())
			  {
				  m_error->setText(m_process->error().empty() ? Wt::WString{"Google sign-in failed."} : m_process->error());
				  return;
			  }

			  const auto username = m_db.username_for_google_sub(identity.id());
			  if(!username)
			  {
				  m_error->setText(
				    "No account is linked to this Google account. Log in with your "
				    "password, then connect it on the Account page.");
				  return;
			  }

			  if(m_db.load_session(*username, m_session))
			  {
				  logged_in.emit();
			  }
			  else
			  {
				  m_error->setText("That account no longer exists.");
			  }
		  });
	}
}

void login_page::submit()
{
	const auto uname = m_username->text().toUTF8();
	const auto pw    = m_password->text().toUTF8();

	m_password->setText("");

	if(m_db.authenticate(uname, pw, m_session))
	{
		logged_in.emit();
	}
	else
	{
		m_error->setText("Invalid credentials.");
	}
}
