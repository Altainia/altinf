#include "account_page.hpp"

#include <Wt/Auth/Identity.h>
#include <Wt/Auth/OAuthService.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "auth/permission.hpp"
#include "auth/widgets/token_manager_widget.hpp"

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
	header->addNew<Wt::WText>(m_session.username, Wt::TextFormat::Plain)
	  ->setStyleClass("account-username");

	m_profile_section = addNew<Wt::WContainerWidget>();
	m_profile_section->setStyleClass("vertical-section");
	render_profile_section();

	m_password_section = addNew<Wt::WContainerWidget>();
	m_password_section->setStyleClass("vertical-section");
	render_password_section();

	m_google_section = addNew<Wt::WContainerWidget>();
	m_google_section->setStyleClass("vertical-section");
	render_google_section();

	if(m_session.permissions.has_any(permission::api_token))
	{
		addNew<token_manager_widget>(m_db, m_session.username, m_session.user_id);
	}
}

void account_page::render_profile_section()
{
	m_profile_section->clear();
	m_profile_section->addNew<Wt::WText>("<h2>Display name</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* field = m_profile_section->addNew<Wt::WLineEdit>(m_session.display_name);
	field->setStyleClass("editor-field");
	field->setPlaceholderText("Display name");

	auto* status = m_profile_section->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	status->setStyleClass("editor-status");

	auto* save = m_profile_section->addNew<Wt::WPushButton>("Save");
	save->setStyleClass("editor-btn");
	save->clicked().connect([this, field, status] {
		m_db.set_display_name(m_session.username, field->text().toUTF8(), m_session.user_id);
		status->setText("Saved.");
	});
}

void account_page::render_password_section()
{
	m_password_section->clear();
	m_password_section->addNew<Wt::WText>("<h2>Password</h2>", Wt::TextFormat::UnsafeXHTML);

	const bool has_pw = m_db.has_password(m_session.username);
	// Only treat Google as a usable fallback when sign-in is actually configured
	// on this server — otherwise removing the password would lock the user out.
	const bool has_google =
	  m_google != nullptr && m_db.google_email_for(m_session.username).has_value();

	auto* status = m_password_section->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	status->setStyleClass("editor-status");

	if(!has_pw)
	{
		// Google-only account: let the user add a password (no current password
		// to confirm, since there is none).
		m_password_section
		  ->addNew<Wt::WText>("No password is set; you sign in with Google.", Wt::TextFormat::Plain)
		  ->setStyleClass("account-note");

		auto* new_pw     = m_password_section->addNew<Wt::WPasswordEdit>();
		auto* confirm_pw = m_password_section->addNew<Wt::WPasswordEdit>();
		for(auto* f: {new_pw, confirm_pw})
		{
			f->setStyleClass("editor-field");
		}
		new_pw->setPlaceholderText("New password");
		confirm_pw->setPlaceholderText("Confirm password");

		auto* set_btn = m_password_section->addNew<Wt::WPushButton>("Set password");
		set_btn->setStyleClass("editor-btn");
		set_btn->clicked().connect([this, new_pw, confirm_pw, status] {
			const auto pw = new_pw->text().toUTF8();
			if(pw.empty())
			{
				status->setText("Enter a new password.");
				return;
			}
			if(pw != confirm_pw->text().toUTF8())
			{
				status->setText("Passwords do not match.");
				return;
			}
			m_db.set_password(m_session.username, pw, m_session.user_id);
			render_password_section();
			render_google_section(); // disconnect becomes available
		});
		return;
	}

	// Has a password: change it (current password required) and, if Google
	// remains as a fallback, allow removing it.
	auto* current_pw = m_password_section->addNew<Wt::WPasswordEdit>();
	auto* new_pw     = m_password_section->addNew<Wt::WPasswordEdit>();
	auto* confirm_pw = m_password_section->addNew<Wt::WPasswordEdit>();
	for(auto* f: {current_pw, new_pw, confirm_pw})
	{
		f->setStyleClass("editor-field");
	}
	current_pw->setPlaceholderText("Current password");
	new_pw->setPlaceholderText("New password");
	confirm_pw->setPlaceholderText("Confirm new password");

	auto* change_btn = m_password_section->addNew<Wt::WPushButton>("Change password");
	change_btn->setStyleClass("editor-btn");
	change_btn->clicked().connect([this, current_pw, new_pw, confirm_pw, status] {
		if(!m_db.verify_password(m_session.username, current_pw->text().toUTF8()))
		{
			status->setText("Current password is incorrect.");
			return;
		}
		const auto pw = new_pw->text().toUTF8();
		if(pw.empty())
		{
			status->setText("Enter a new password.");
			return;
		}
		if(pw != confirm_pw->text().toUTF8())
		{
			status->setText("Passwords do not match.");
			return;
		}
		m_db.set_password(m_session.username, pw, m_session.user_id);
		render_password_section();
	});

	if(has_google)
	{
		auto* remove_btn = m_password_section->addNew<Wt::WPushButton>("Remove password");
		remove_btn->setStyleClass("editor-btn editor-btn-danger");
		remove_btn->clicked().connect([this, current_pw, status] {
			if(!m_db.verify_password(m_session.username, current_pw->text().toUTF8()))
			{
				status->setText("Enter your current password to remove it.");
				return;
			}
			if(!m_db.unset_password(m_session.username, m_session.user_id))
			{
				status->setText("Cannot remove your password.");
				return;
			}
			render_password_section();
			render_google_section(); // Google is now the only sign-in method
		});
	}
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

		// Disconnecting is only safe while a password remains as a fallback.
		if(m_db.has_password(m_session.username))
		{
			auto* disconnect = m_google_section->addNew<Wt::WPushButton>("Disconnect");
			disconnect->setStyleClass("editor-btn editor-btn-danger");
			disconnect->clicked().connect([this] {
				m_db.unlink_google(m_session.username, m_session.user_id);
				render_google_section();
				render_password_section(); // removing-password option goes away
			});
		}
		else
		{
			m_google_section
			  ->addNew<Wt::WText>("Set a password before disconnecting Google.",
			                      Wt::TextFormat::Plain)
			  ->setStyleClass("account-google-note");
		}
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

	auto* connect = m_google_section->addNew<Wt::WPushButton>("Connect Google account");
	connect->setStyleClass("editor-btn");
	connect->clicked().connect(m_process.get(), &Wt::Auth::OAuthProcess::startAuthenticate);

	auto* error = m_google_section->addNew<Wt::WText>();
	error->setStyleClass("account-google-error");

	m_process->authenticated().connect([this, error](const Wt::Auth::Identity& identity) {
		if(!identity.isValid())
		{
			error->setText(m_process->error().empty() ? Wt::WString{"Google sign-in failed. Please try again."} : m_process->error());
			return;
		}

		const auto owner = m_db.username_for_google_sub(identity.id());
		if(owner && *owner != m_session.username)
		{
			error->setText("This Google account is already linked to another account.");
			return;
		}

		m_db.link_google(m_session.username, identity.id(), identity.email(), m_session.user_id);
		render_google_section();
		render_password_section(); // removing-password option becomes available
	});
}
