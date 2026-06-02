#include "account_edit_page.hpp"

#include <Wt/WApplication.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "auth/permission.hpp"
#include "auth/widgets/token_manager_widget.hpp"
#include "paths.hpp"
#include "widgets/live_hub.hpp"

account_edit_page::account_edit_page(user_db* db, const session_data& session, const user_entry* existing):
  m_db{db},
  m_session{session}
{
	if(existing)
	{
		m_existing = *existing;
	}

	setStyleClass("page account-editor-page");

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("post-editor-form");

	form->addNew<Wt::WText>(m_existing ? "<h2>Edit User</h2>" : "<h2>New User</h2>",
	                        Wt::TextFormat::UnsafeXHTML);

	m_username = form->addNew<Wt::WLineEdit>();
	m_username->setStyleClass("editor-field");
	m_username->setPlaceholderText("Username (required)");
	if(m_existing)
	{
		m_username->setText(m_existing->username);
		m_username->setEnabled(false);
	}

	m_display_name = form->addNew<Wt::WLineEdit>();
	m_display_name->setStyleClass("editor-field");
	m_display_name->setPlaceholderText("Display name (optional)");
	if(m_existing)
	{
		m_display_name->setText(m_existing->display_name);
	}

	m_password = form->addNew<Wt::WPasswordEdit>();
	m_password->setStyleClass("editor-field");
	m_password->setPlaceholderText(m_existing ? "New password (leave blank to keep current)" : "Password (required)");

	m_password_confirm = form->addNew<Wt::WPasswordEdit>();
	m_password_confirm->setStyleClass("editor-field");
	m_password_confirm->setPlaceholderText("Confirm password");

	auto* perms_section = form->addNew<Wt::WContainerWidget>();
	perms_section->setStyleClass("account-perms-section");
	perms_section->addNew<Wt::WText>("<p class=\"account-perms-label\">Permissions</p>",
	                                 Wt::TextFormat::UnsafeXHTML);

	const auto cur_perms = m_existing ? m_existing->permissions : permission::flags{};

	m_perm_admin = perms_section->addNew<Wt::WCheckBox>("Admin");
	m_perm_admin->setChecked(cur_perms.has_any(permission::admin));

	m_perm_manage_users = perms_section->addNew<Wt::WCheckBox>("Manage Users");
	m_perm_manage_users->setChecked(cur_perms.has_any(permission::manage_users));

	m_perm_post_write = perms_section->addNew<Wt::WCheckBox>("Write Posts");
	m_perm_post_write->setChecked(cur_perms.has_any(permission::post_write));

	m_perm_org_create = perms_section->addNew<Wt::WCheckBox>("Create Orgs");
	m_perm_org_create->setChecked(cur_perms.has_any(permission::org_create));

	m_perm_api_token = perms_section->addNew<Wt::WCheckBox>("API Tokens");
	m_perm_api_token->setChecked(cur_perms.has_any(permission::api_token));

	m_perm_view_history = perms_section->addNew<Wt::WCheckBox>("View History");
	m_perm_view_history->setChecked(cur_perms.has_any(permission::view_user_history));

	m_status = form->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status->setStyleClass("editor-status");

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	auto* save_btn = btn_row->addNew<Wt::WPushButton>("Save");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect(this, &account_edit_page::save);

	auto* cancel_btn = btn_row->addNew<Wt::WPushButton>("Cancel");
	cancel_btn->setStyleClass("editor-btn editor-btn-cancel");
	cancel_btn->clicked().connect([] {
		Wt::WApplication::instance()->setInternalPath(paths::account_list(), true);
	});

	if(m_existing)
	{
		const auto target = m_existing->username;

		// Google: an admin may unset another user's Google link, but only while a
		// password remains so the user is not locked out.
		if(m_db->google_email_for(target))
		{
			auto* google_section = addNew<Wt::WContainerWidget>();
			google_section->setStyleClass("vertical-section");
			google_section->addNew<Wt::WText>("<h3>Google</h3>", Wt::TextFormat::UnsafeXHTML);

			if(m_db->has_password(target))
			{
				auto* unset = google_section->addNew<Wt::WPushButton>("Unset Google");
				unset->setStyleClass("editor-btn editor-btn-danger");
				unset->clicked().connect([this, target, google_section] {
					m_db->unlink_google(target, m_session.user_id);
					live_hub::instance().broadcast("accounts");
					google_section->hide();
				});
			}
			else
			{
				google_section
				  ->addNew<Wt::WText>(
				    "This user signs in with Google only; set a password "
				    "before unsetting it.",
				    Wt::TextFormat::Plain)
				  ->setStyleClass("account-note");
			}
		}

		// History: visible to admins with the view_user_history permission.
		if(m_session.permissions.has_any(permission::view_user_history))
		{
			auto* hist_btn = addNew<Wt::WPushButton>("View history");
			hist_btn->setStyleClass("editor-btn");
			hist_btn->clicked().connect([target] {
				Wt::WApplication::instance()->setInternalPath(paths::account_history(target), true);
			});
		}

		addNew<token_manager_widget>(*m_db, target, m_session.user_id);
	}
}

void account_edit_page::save()
{
	const auto username     = m_username->text().toUTF8();
	const auto display_name = m_display_name->text().toUTF8();
	const auto password     = m_password->text().toUTF8();
	const auto pw_confirm   = m_password_confirm->text().toUTF8();

	if(username.empty())
	{
		m_status->setText("Username is required.");
		return;
	}

	if(!m_existing && password.empty())
	{
		m_status->setText("Password is required for new users.");
		return;
	}

	if(!password.empty() && password != pw_confirm)
	{
		m_status->setText("Passwords do not match.");
		m_password->setText("");
		m_password_confirm->setText("");
		return;
	}

	permission::flags perms;
	if(m_perm_admin->isChecked())
	{
		perms |= permission::admin;
	}
	if(m_perm_manage_users->isChecked())
	{
		perms |= permission::manage_users;
	}
	if(m_perm_post_write->isChecked())
	{
		perms |= permission::post_write;
	}
	if(m_perm_org_create->isChecked())
	{
		perms |= permission::org_create;
	}
	if(m_perm_api_token->isChecked())
	{
		perms |= permission::api_token;
	}
	if(m_perm_view_history->isChecked())
	{
		perms |= permission::view_user_history;
	}

	if(m_existing)
	{
		m_db->update_user(username, display_name, perms, m_session.user_id);
		if(!password.empty())
		{
			m_db->set_password(username, password, m_session.user_id);
		}
	}
	else
	{
		if(m_db->username_exists(username))
		{
			m_status->setText("Username already exists.");
			return;
		}
		m_db->create_user(username, password, perms, display_name, m_session.user_id);
	}

	live_hub::instance().broadcast("accounts");
	saved.emit();
}
