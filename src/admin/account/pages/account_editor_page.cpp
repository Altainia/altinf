#include "account_editor_page.hpp"

#include <Wt/WApplication.h>
#include <Wt/WDialog.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "auth/permission.hpp"
#include "widgets/live_hub.hpp"

account_editor_page::account_editor_page(user_db*              db,
                                         const user_entry*     existing,
                                         std::function<void()> on_save):
  m_db{db},
  m_on_save{std::move(on_save)}
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

	if(m_existing)
	{
		auto* tokens_section = addNew<Wt::WContainerWidget>();
		tokens_section->setStyleClass("account-tokens-section");
		tokens_section->addNew<Wt::WText>("<h3>API Tokens</h3>", Wt::TextFormat::UnsafeXHTML);

		m_tokens_container = tokens_section->addNew<Wt::WContainerWidget>();
		build_token_list();

		auto* gen_btn = tokens_section->addNew<Wt::WPushButton>("Generate New Token");
		gen_btn->setStyleClass("editor-btn");
		gen_btn->clicked().connect(this, &account_editor_page::generate_token);
	}
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

	if(m_existing)
	{
		m_db->update_user(username, display_name, perms);
		if(!password.empty())
		{
			m_db->set_password(username, password);
		}
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

	live_hub::instance().broadcast("accounts");
	m_on_save();
}

void account_editor_page::build_token_list()
{
	m_tokens_container->clear();

	const auto tokens = m_db->list_tokens(m_existing->username);

	if(tokens.empty())
	{
		auto* empty = m_tokens_container->addNew<Wt::WText>("No tokens.", Wt::TextFormat::Plain);
		empty->setStyleClass("account-tokens-empty");
		return;
	}

	for(const auto& tok: tokens)
	{
		auto* row = m_tokens_container->addNew<Wt::WContainerWidget>();
		row->setStyleClass("account-token-row");

		const auto display   = tok.token_hash.substr(0, 12) + "...";
		auto*      hash_text = row->addNew<Wt::WText>(display, Wt::TextFormat::Plain);
		hash_text->setStyleClass("account-token-hash");

		const long long token_id = tok.id;
		auto*           rev_btn  = row->addNew<Wt::WPushButton>("Revoke");
		rev_btn->setStyleClass("link-action-btn link-delete-btn");
		rev_btn->clicked().connect([this, token_id] {
			auto* d = new Wt::WDialog("Revoke Token");
			d->contents()->addNew<Wt::WText>("Revoke this API token? This cannot be undone.",
			                                 Wt::TextFormat::Plain);
			auto* yes = d->footer()->addNew<Wt::WPushButton>("Revoke");
			yes->setStyleClass("editor-btn");
			auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
			no->setStyleClass("editor-btn editor-btn-cancel");
			yes->clicked().connect([this, d, token_id] {
				d->accept();
				m_db->delete_token(token_id);
				build_token_list();
			});
			no->clicked().connect([d] { d->reject(); });
			d->finished().connect([d](Wt::DialogCode) { delete d; });
			d->show();
		});
	}
}

void account_editor_page::generate_token()
{
	const auto raw_token = m_db->create_api_token(m_existing->username);
	build_token_list();

	auto* d = new Wt::WDialog("New API Token");
	d->contents()->addNew<Wt::WText>(
	  "<p>Copy this token now — it will <strong>not</strong> be shown again.</p>",
	  Wt::TextFormat::UnsafeXHTML);
	auto* field = d->contents()->addNew<Wt::WLineEdit>(raw_token);
	field->setReadOnly(true);
	field->setStyleClass("editor-field token-display-field");
	auto* ok = d->footer()->addNew<Wt::WPushButton>("Done");
	ok->setStyleClass("editor-btn");
	ok->clicked().connect([d] { d->accept(); });
	d->finished().connect([d](Wt::DialogCode) { delete d; });
	d->show();
}
