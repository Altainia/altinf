#include "account_page.hpp"

#include <Wt/Auth/Identity.h>
#include <Wt/Auth/OAuthService.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

// Defined here, where Wt::Auth::OAuthService/OAuthProcess are complete types, so
// the m_process unique_ptr can be destroyed.
account_page::~account_page() = default;

account_page::account_page(user_db&                      db,
                           const session_data&           session,
                           const Wt::Auth::OAuthService* google):
  m_db{db},
  m_session{session},
  m_google{google}
{
	setStyleClass("page account-page");

	auto* header = addNew<Wt::WContainerWidget>();
	header->setStyleClass("vertical-section");
	header->addNew<Wt::WText>("<h1>Account</h1>", Wt::TextFormat::UnsafeXHTML);
	header->addNew<Wt::WText>(m_session.display_name.empty() ? m_session.username : m_session.display_name,
	                          Wt::TextFormat::Plain)
	  ->setStyleClass("account-username");

	m_google_section = addNew<Wt::WContainerWidget>();
	m_google_section->setStyleClass("vertical-section");
	render_google_section();
}

void account_page::render_google_section()
{
	m_google_section->clear();
	m_google_section->addNew<Wt::WText>("<h2>Google</h2>", Wt::TextFormat::UnsafeXHTML);

	if(m_google == nullptr)
	{
		m_google_section
		  ->addNew<Wt::WText>("Google sign-in is not configured on this server.",
		                      Wt::TextFormat::Plain)
		  ->setStyleClass("account-google-note");
		return;
	}

	if(const auto email = m_db.google_email_for(m_session.username))
	{
		m_google_section
		  ->addNew<Wt::WText>("Connected as " + *email, Wt::TextFormat::Plain)
		  ->setStyleClass("account-google-status");

		auto* disconnect =
		  m_google_section->addNew<Wt::WPushButton>("Disconnect");
		disconnect->setStyleClass("editor-btn editor-btn-danger");
		disconnect->clicked().connect([this] {
			m_db.unlink_google(m_session.username);
			render_google_section();
		});
		return;
	}

	m_google_section
	  ->addNew<Wt::WText>("Connect your Google account to sign in faster next time.",
	                      Wt::TextFormat::Plain)
	  ->setStyleClass("account-google-note");

	start_connect();
}

void account_page::start_connect()
{
	// A fresh process per attempt; owned by the page so the redirect callback
	// outlives the click handler.
	m_process = m_google->createProcess(m_google->authenticationScope());

	auto* connect =
	  m_google_section->addNew<Wt::WPushButton>("Connect Google account");
	connect->setStyleClass("editor-btn");
	connect->clicked().connect(m_process.get(),
	                           &Wt::Auth::OAuthProcess::startAuthenticate);

	auto* error = m_google_section->addNew<Wt::WText>();
	error->setStyleClass("account-google-error");

	m_process->authenticated().connect(
	  [this, error](const Wt::Auth::Identity& identity) {
		  if(!identity.isValid())
		  {
			  error->setText(m_process->error().empty() ? Wt::WString{"Google sign-in failed. Please try again."} : m_process->error());
			  return;
		  }

		  const auto owner = m_db.username_for_google_sub(identity.id());
		  if(owner && *owner != m_session.username)
		  {
			  error->setText(
			    "This Google account is already linked to another account.");
			  return;
		  }

		  m_db.link_google(m_session.username, identity.id(), identity.email());
		  render_google_section();
	  });
}
