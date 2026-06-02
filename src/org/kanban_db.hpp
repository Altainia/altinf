#pragma once

#include <Wt/Dbo/Session.h>

#include <optional>
#include <string>
#include <vector>

#include "auth/user_lookup.hpp"
#include "org/kanban.hpp"

class kanban_db
{
public:
	explicit kanban_db(const std::string& db_path);

	// Display name + soft-deleted flag for a username, for rendering member,
	// assignee, comment-author and history lists.
	user_lookup::info resolve_user(const std::string& username);

	// Teams
	long long                 create_team(const std::string& name, long long org_id);
	void                      rename_team(long long id, const std::string& name);
	void                      set_team_org(long long team_id, long long org_id);
	std::vector<team_entry>   all_teams();
	std::vector<team_entry>   teams_for_org(long long org_id);
	std::optional<team_entry> find_team(long long id);
	void                      archive_team(long long id, const std::string& actor);
	void                      unarchive_team(long long id);
	std::vector<team_entry>   archived_teams_for_org(long long org_id);

	// Members
	void                           add_member(long long team_id, const std::string& username);
	void                           remove_member(long long team_id, const std::string& username);
	void                           remove_member_from_org_teams(long long          org_id,
	                                                            const std::string& username);
	void                           remove_member_from_all_teams(const std::string& username);
	void                           set_team_lead(long long          team_id,
	                                             const std::string& username,
	                                             bool               is_lead);
	bool                           is_team_lead(long long team_id, const std::string& username);
	std::vector<std::string>       members_for_team(long long team_id);
	std::vector<team_member_entry> team_member_entries(long long team_id);
	bool                           is_member(long long team_id, const std::string& username);
	std::vector<long long>         team_ids_for_user(const std::string& username);

	// Tasks
	long long add_task(const kanban_task_entry& e,
	                   const std::string&       actor);
	void      update_task(const kanban_task_entry& e,
	                      const std::string&       actor);
	void      update_task_status(long long          id,
	                             const std::string& status,
	                             int                sort_order,
	                             const std::string& actor);
	// Returns false if the task is in "done", is archived, or username is already assigned.
	bool add_assignee(long long task_id, const std::string& username, const std::string& actor);
	// Returns false if username is not currently an assignee or task is archived.
	bool                             remove_assignee(long long task_id, const std::string& username, const std::string& actor);
	std::vector<std::string>         assignees_for_task(long long task_id);
	void                             archive_task(long long id, const std::string& actor);
	void                             unarchive_task(long long id, const std::string& actor);
	std::optional<kanban_task_entry> find_task(long long id);
	std::vector<kanban_task_entry>   tasks_for_team(long long team_id);
	std::vector<kanban_task_entry>   archived_tasks_for_team(long long team_id);
	std::vector<task_event_entry>    history_for_task(long long task_id);

	// Comments
	long long                       add_comment(long long          task_id,
	                                            const std::string& author,
	                                            const std::string& body);
	void                            edit_comment(long long          comment_id,
	                                             const std::string& editor,
	                                             const std::string& new_body);
	void                            delete_comment(long long          comment_id,
	                                               const std::string& actor);
	std::vector<task_comment_entry> comments_for_task(long long task_id);

	// Team settings
	team_settings_entry                    settings_for_team(long long team_id);
	void                                   set_team_settings(const team_settings_entry& s,
	                                                         const std::string&         actor);
	std::vector<team_settings_event_entry> settings_events_for_team(long long team_id);

	// Task types
	long long                      create_task_type(long long          org_id,
	                                                const std::string& name,
	                                                const std::string& color);
	void                           update_task_type(long long          id,
	                                                const std::string& name,
	                                                const std::string& color);
	void                           delete_task_type(long long id);
	std::vector<task_type_entry>   types_for_org(long long org_id);
	std::optional<task_type_entry> find_task_type(long long id);

private:
	Wt::Dbo::Session m_dbo;

	// username <-> user_id resolution against the shared user table.
	long long   uid_of(const std::string& username);
	std::string username_of(long long user_id);
	// Batch id -> username for list reads (one query instead of one per row).
	std::unordered_map<long long, std::string> usernames_of(const std::vector<long long>& ids);

	static team_entry        to_entry(const Wt::Dbo::ptr<team_record>& p);
	static kanban_task_entry to_entry(const Wt::Dbo::ptr<kanban_task_record>& p);
	static task_type_entry   to_entry(const Wt::Dbo::ptr<task_type_record>& p);

	// Build task entries and fill their assignees, fetching every task's assignees
	// in one query and resolving all names in one more (instead of per task/row).
	std::vector<kanban_task_entry>
	  attach_assignees(const std::vector<Wt::Dbo::ptr<kanban_task_record>>& tasks);

	void record_event(long long                                   task_id,
	                  const std::string&                          actor,
	                  const std::string&                          event_type,
	                  const std::vector<task_field_change_entry>& changes);
	void record_comment_event(long long          comment_id,
	                          const std::string& actor,
	                          const std::string& event_type,
	                          const std::string& body_snapshot);
	void maybe_clear_assignees_for_done(long long          task_id,
	                                    const std::string& new_status,
	                                    const std::string& actor);
};