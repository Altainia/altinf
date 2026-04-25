#include "account_editor_page.h"

#include "auth/permission.h"

#include <Wt/WApplication.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

account_editor_page::account_editor_page(user_db*              db,
                                         const user_entry*     existing,
                                         std::function<void()> on_save):
  m_db{db},
  m_on_save{std::move(on_save)}
{
	if(existing)
		m_existing = *existing;

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
		m_display_name->setText(m_existing->display_name);

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

	const uint64_t cur_perms = m_existing ? m_existing->permissions : 0;

	m_perm_admin = perms_section->addNew<Wt::WCheckBox>("Admin");
	m_perm_admin->setChecked(has_permission(cur_perms, permission::admin));

	m_perm_manage_users = perms_section->addNew<Wt::WCheckBox>("Manage Users");
	m_perm_manage_users->setChecked(has_permission(cur_perms, permission::manage_users));

	m_perm_post_write = perms_section->addNew<Wt::WCheckBox>("Write Posts");
	m_perm_post_write->setChecked(has_permission(cur_perms, permission::post_write));

	m_perm_gantt_write = perms_section->addNew<Wt::WCheckBox>("Write Gantt");
	m_perm_gantt_write->setChecked(has_permission(cur_perms, permission::gantt_write));

	m_status = form->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status->setStyleClass("editor-status");

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	auto* save_btn = btn_row->addNew<Wt::WPushButton>("Save");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect(this, &account_editor_page::save);

	auto* cancel_btn = btn_row->addNew<Wt::WPushButton>("Cancel");
	cancel_btn->setStyleClass("editor-btn editor-btn-cancel");
	cancel_btn->clicked().connect([] {
		Wt::WApplication::instance()->setInternalPath("/admin/accounts", true);
	});
}

void account_editor_page::save()
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

	uint64_t perms = 0;
	if(m_perm_admin->isChecked())
		perms = grant(perms, permission::admin);
	if(m_perm_manage_users->isChecked())
		perms = grant(perms, permission::manage_users);
	if(m_perm_post_write->isChecked())
		perms = grant(perms, permission::post_write);
	if(m_perm_gantt_write->isChecked())
		perms = grant(perms, permission::gantt_write);

	if(m_existing)
	{
		m_db->update_user(username, display_name, perms);
		if(!password.empty())
			m_db->set_password(username, password);
	}
	else
	{
		if(m_db->username_exists(username))
		{
			m_status->setText("Username already exists.");
			return;
		}
		m_db->create_user(username, password, perms, display_name);
	}

	m_on_save();
}
