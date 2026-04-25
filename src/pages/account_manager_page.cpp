#include "account_manager_page.h"

#include "auth/permission.h"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WDialog.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>

#include <string>
#include <vector>

static std::string permissions_label(uint64_t perms)
{
	std::vector<std::string> parts;
	if(has_permission(perms, permission::admin))
		parts.emplace_back("Admin");
	if(has_permission(perms, permission::manage_users))
		parts.emplace_back("Manage Users");
	if(has_permission(perms, permission::post_write))
		parts.emplace_back("Write Posts");
	if(has_permission(perms, permission::gantt_write))
		parts.emplace_back("Write Gantt");
	if(parts.empty())
		return "None";
	std::string out;
	for(std::size_t i = 0; i < parts.size(); ++i)
	{
		if(i)
			out += ", ";
		out += parts[i];
	}
	return out;
}

account_manager_page::account_manager_page(
  user_db&                                db,
  const session_data&                     session,
  std::function<void(const std::string&)> on_delete):
  m_db{db},
  m_session{session},
  m_on_delete{std::move(on_delete)}
{
	setStyleClass("page account-manager-page");

	addNew<Wt::WText>("<h1>Accounts</h1>", Wt::TextFormat::UnsafeXHTML);

	auto* new_btn = addNew<Wt::WPushButton>("New User");
	new_btn->setStyleClass("editor-btn account-new-btn");
	new_btn->clicked().connect([] {
		Wt::WApplication::instance()->setInternalPath("/admin/accounts/new", true);
	});

	const auto users = m_db.list_users();
	if(users.empty())
	{
		addNew<Wt::WText>("<p class=\"accounts-empty\">No users found.</p>",
		                  Wt::TextFormat::UnsafeXHTML);
		return;
	}

	auto* tbl = addNew<Wt::WTable>();
	tbl->setStyleClass("account-table");
	tbl->setHeaderCount(1);

	tbl->elementAt(0, 0)->addNew<Wt::WText>("Username", Wt::TextFormat::Plain);
	tbl->elementAt(0, 1)->addNew<Wt::WText>("Display Name", Wt::TextFormat::Plain);
	tbl->elementAt(0, 2)->addNew<Wt::WText>("Permissions", Wt::TextFormat::Plain);
	tbl->elementAt(0, 3)->addNew<Wt::WText>("Actions", Wt::TextFormat::Plain);

	for(int row = 0; row < static_cast<int>(users.size()); ++row)
	{
		const auto& u     = users[row];
		const bool  is_me = (u.username == m_session.username);

		tbl->elementAt(row + 1, 0)->addNew<Wt::WText>(u.username, Wt::TextFormat::Plain);
		tbl->elementAt(row + 1, 1)->addNew<Wt::WText>(u.display_name.empty() ? "—" : u.display_name, Wt::TextFormat::Plain);
		tbl->elementAt(row + 1, 2)->addNew<Wt::WText>(permissions_label(u.permissions), Wt::TextFormat::Plain);

		auto* actions = tbl->elementAt(row + 1, 3)->addNew<Wt::WContainerWidget>();
		actions->setStyleClass("account-actions");

		auto* edit_anchor = actions->addNew<Wt::WAnchor>(
		  Wt::WLink{Wt::LinkType::InternalPath, "/admin/accounts/edit/" + u.username}, "Edit");
		edit_anchor->setStyleClass("link-action-link");

		if(is_me)
		{
			auto* note =
			  actions->addNew<Wt::WText>("cannot delete own account", Wt::TextFormat::Plain);
			note->setStyleClass("account-self-note");
		}
		else
		{
			const std::string del_username = u.username;
			auto*             del_btn      = actions->addNew<Wt::WPushButton>("Delete");
			del_btn->setStyleClass("link-action-btn link-delete-btn");
			del_btn->clicked().connect([this, del_username] {
				auto* d = new Wt::WDialog("Confirm Delete");
				d->contents()->addNew<Wt::WText>(
				  "Delete user \"" + del_username + "\"? This cannot be undone.",
				  Wt::TextFormat::Plain);
				auto* yes = d->footer()->addNew<Wt::WPushButton>("Delete");
				yes->setStyleClass("editor-btn");
				auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
				no->setStyleClass("editor-btn editor-btn-cancel");
				yes->clicked().connect([this, d, del_username] {
					d->accept();
					m_on_delete(del_username);
				});
				no->clicked().connect([d] { d->reject(); });
				d->finished().connect([d](Wt::DialogCode) { delete d; });
				d->show();
			});
		}
	}
}
