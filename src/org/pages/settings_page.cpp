#include "settings_page.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WText.h>

#include "org/org.hpp"

settings_page::settings_page(org_db& odb, const session_data& session)
{
	setStyleClass("page settings-page");
	addNew<Wt::WText>("<h1>Settings</h1>", Wt::TextFormat::UnsafeXHTML);

	const auto orgs = odb.orgs_for_user(session.username);
	if(orgs.empty())
	{
		addNew<Wt::WText>(
		  "You are not a member of any organizations.", Wt::TextFormat::Plain)
		  ->setStyleClass("settings-empty");
		return;
	}

	for(const auto& org: orgs)
	{
		auto* section = addNew<Wt::WContainerWidget>();
		section->setStyleClass("settings-org-section");
		section->addNew<Wt::WText>("<h2>" + org.name + "</h2>",
		                           Wt::TextFormat::UnsafeXHTML);

		const long long org_id   = org.id;
		const auto      username = session.username;
		const auto      pref     = odb.get_user_org_pref(username, org_id);

		auto* available_cb  = section->addNew<Wt::WCheckBox>("New task available");
		auto* unassigned_cb = section->addNew<Wt::WCheckBox>("Removed from task");
		auto* coassignee_cb = section->addNew<Wt::WCheckBox>("Co-assignee changed");
		auto* abandoned_cb  = section->addNew<Wt::WCheckBox>("Task abandoned");

		available_cb->setChecked(pref.notify_task_available);
		unassigned_cb->setChecked(pref.notify_task_unassigned);
		coassignee_cb->setChecked(pref.notify_coassignee_changed);
		abandoned_cb->setChecked(pref.notify_task_abandoned);

		for(auto* cb: {available_cb, unassigned_cb, coassignee_cb, abandoned_cb})
		{
			cb->setStyleClass("settings-pref-check");
		}

		available_cb->changed().connect(
		  [&odb, username, org_id, available_cb] {
			  auto p                  = odb.get_user_org_pref(username, org_id);
			  p.notify_task_available = available_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
		unassigned_cb->changed().connect(
		  [&odb, username, org_id, unassigned_cb] {
			  auto p                   = odb.get_user_org_pref(username, org_id);
			  p.notify_task_unassigned = unassigned_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
		coassignee_cb->changed().connect(
		  [&odb, username, org_id, coassignee_cb] {
			  auto p                      = odb.get_user_org_pref(username, org_id);
			  p.notify_coassignee_changed = coassignee_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
		abandoned_cb->changed().connect(
		  [&odb, username, org_id, abandoned_cb] {
			  auto p                  = odb.get_user_org_pref(username, org_id);
			  p.notify_task_abandoned = abandoned_cb->isChecked();
			  odb.set_user_org_pref(p);
		  });
	}
}