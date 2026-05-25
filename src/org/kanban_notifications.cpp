#include "kanban_notifications.hpp"

#include <alt/functional.hpp>
#include <ranges>

#include "org/org.hpp"
#include "widgets/live_hub.hpp"

void notify_assignee_added(kanban_db&         db,
                           org_db&            odb,
                           long long          task_id,
                           long long          team_id,
                           long long          org_id,
                           const std::string& added_user,
                           const std::string& actor)
{
	const auto        task_opt   = db.find_task(task_id);
	const auto        team_opt   = db.find_team(team_id);
	const std::string task_title = task_opt ? task_opt->title : "";
	const std::string team_name  = team_opt ? team_opt->name : "";

	if(added_user != actor)
	{
		odb.push_notification(
		  added_user, "task_assigned", make_task_assigned_payload(task_id, task_title, team_id, team_name));
		live_hub::instance().broadcast("user:" + added_user);
	}

	const auto all_assignees = db.assignees_for_task(task_id);
	for(const auto& u: all_assignees | std::views::filter(alt::not_equals{added_user}))
	{
		const auto pref = odb.get_user_org_pref(u, org_id);
		if(pref.notify_coassignee_changed)
		{
			odb.push_notification(
			  u, "task_coassignee_changed", make_task_coassignee_changed_payload(task_id, task_title, team_id, team_name, added_user, "added"));
			live_hub::instance().broadcast("user:" + u);
		}
	}
}

void notify_assignee_removed(kanban_db&                      db,
                             org_db&                         odb,
                             long long                       task_id,
                             long long                       team_id,
                             long long                       org_id,
                             const std::string&              removed_user,
                             const std::string&              actor,
                             const std::vector<std::string>& remaining_assignees)
{
	const auto        task_opt   = db.find_task(task_id);
	const auto        team_opt   = db.find_team(team_id);
	const std::string task_title = task_opt ? task_opt->title : "";
	const std::string team_name  = team_opt ? team_opt->name : "";

	if(actor != removed_user)
	{
		const auto pref = odb.get_user_org_pref(removed_user, org_id);
		if(pref.notify_task_unassigned)
		{
			odb.push_notification(
			  removed_user, "task_unassigned", make_task_unassigned_payload(task_id, task_title, team_id, team_name));
			live_hub::instance().broadcast("user:" + removed_user);
		}
	}

	if(remaining_assignees.empty())
	{
		const auto members = db.members_for_team(team_id);
		for(const auto& u: members | std::views::filter(alt::not_equals{removed_user}))
		{
			const auto pref = odb.get_user_org_pref(u, org_id);

			if(actor == removed_user && pref.notify_task_abandoned)
			{
				odb.push_notification(
				  u, "task_abandoned", make_task_abandoned_payload(task_id, task_title, team_id, team_name, removed_user));
				live_hub::instance().broadcast("user:" + u);
			}

			if(pref.notify_task_available)
			{
				odb.push_notification(
				  u, "task_available", make_task_available_payload(task_id, task_title, team_id, team_name));
				live_hub::instance().broadcast("user:" + u);
			}
		}
	}

	for(const auto& u: remaining_assignees)
	{
		const auto pref = odb.get_user_org_pref(u, org_id);
		if(pref.notify_coassignee_changed)
		{
			odb.push_notification(
			  u, "task_coassignee_changed", make_task_coassignee_changed_payload(task_id, task_title, team_id, team_name, removed_user, "removed"));
			live_hub::instance().broadcast("user:" + u);
		}
	}
}