#pragma once

#include "org/kanban_db.hpp"
#include "org/org_db.hpp"

#include <string>
#include <vector>

// Call after a successful add_assignee().
// Fires task_assigned to added_user (if actor != added_user) and
// task_coassignee_changed to all other current assignees who opt in.
void notify_assignee_added(kanban_db&         db,
                           org_db&            odb,
                           long long          task_id,
                           long long          team_id,
                           long long          org_id,
                           const std::string& added_user,
                           const std::string& actor);

// Call after a successful remove_assignee().
// remaining_assignees: result of db.assignees_for_task(task_id) after removal.
// Fires task_unassigned, task_abandoned, task_available, task_coassignee_changed
// according to spec rules (see kanban_notifications.cpp).
void notify_assignee_removed(kanban_db&                      db,
                             org_db&                         odb,
                             long long                       task_id,
                             long long                       team_id,
                             long long                       org_id,
                             const std::string&              removed_user,
                             const std::string&              actor,
                             const std::vector<std::string>& remaining_assignees);
