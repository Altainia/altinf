#include "login_page.h"

#include <Wt/WPushButton.h>
#include <Wt/WText.h>

login_page::login_page(user_db&              db,
                       session_data&         session,
                       std::function<void()> on_login):
  m_db{db},
  m_session{session},
  m_on_login{std::move(on_login)}
{
	setStyleClass("page login-page");

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("login-form");

	form->addNew<Wt::WText>("<h2>Login</h2>", Wt::TextFormat::UnsafeXHTML);

	m_username = form->addNew<Wt::WLineEdit>();
	m_username->setPlaceholderText("Username");
	m_username->setStyleClass("login-field");

	m_password = form->addNew<Wt::WLineEdit>();
	m_password->setPlaceholderText("Password");
	m_password->setEchoMode(Wt::EchoMode::Password);
	m_password->setStyleClass("login-field");

	m_error = form->addNew<Wt::WText>();
	m_error->setStyleClass("login-error");

	auto* submit_btn = form->addNew<Wt::WPushButton>("Login");
	submit_btn->setStyleClass("login-btn");
	submit_btn->clicked().connect(this, &login_page::submit);

	m_username->enterPressed().connect(this, &login_page::submit);
	m_password->enterPressed().connect(this, &login_page::submit);
}

void login_page::submit()
{
	const auto uname = m_username->text().toUTF8();
	const auto pw    = m_password->text().toUTF8();

	m_password->setText("");

	if(m_db.authenticate(uname, pw, m_session))
	{
		m_on_login();
	}
	else
	{
		m_error->setText("Invalid credentials.");
	}
}
