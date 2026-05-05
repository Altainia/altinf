#pragma once

#include "kanban.hpp"
#include "auth/permission.hpp"

#include <Wt/Dbo/Session.h>

#include <optional>
#include <string>
#include <vector>

class kanban_db
{
public:
	explicit kanban_db(const std::string& db_path);

	// Teams
	long long                 create_team(const std::string& name, long long org_id);
	void                      rename_team(long long id, const std::string& name);
	void                      set_team_org(long long team_id, long long org_id);
	std::vector<team_entry>   all_teams();
	std::vector<team_entry>   teams_for_org(long long org_id);
	std::optional<team_entry> find_team(long long id);
	void                      delete_team(long long id);

	// Members
	void                           add_member(long long team_id, const std::string& username);
	void                           remove_member(long long team_id, const std::string& username);
	void                           remove_member_from_org_teams(long long          org_id,
	                                                            const std::string& username);
	void                           remove_member_from_all_teams(const std::string& username);
	void                           set_team_lead(long long team_id,
	                                             const std::string& username,
	                                             bool               is_lead);
	bool                           is_team_lead(long long team_id, const std::string& username);
	std::vector<std::string>       members_for_team(long long team_id);
	std::vector<team_member_entry> team_member_entries(long long team_id);
	bool                           is_member(long long team_id, const std::string& username);
	std::vector<long long>         team_ids_for_user(const std::string& username);

	// Tasks
	long long                        add_task(const kanban_task_entry& e);
	void                             update_task(const kanban_task_entry& e);
	void                             update_task_status(long long          id,
	                                                    const std::string& status,
	                                                    int                sort_order);
	// Assigns an unassigned task to username; no-op if already assigned.
	bool                             self_assign(long long task_id, const std::string& username);
	void                             delete_task(long long id);
	std::optional<kanban_task_entry> find_task(long long id);
	std::vector<kanban_task_entry>   tasks_for_team(long long team_id);

	// Permission helpers (is_org_lead pre-computed by the caller)
	bool can_view_board(long long team_id, const std::string& username,
	                    permission::flags perms, bool is_org_lead = false);
	bool can_edit_board(long long team_id, const std::string& username,
	                    permission::flags perms, bool is_org_lead = false,
	                    bool is_team_lead = false);

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

	static team_entry        to_entry(const Wt::Dbo::ptr<team_record>& p);
	static kanban_task_entry to_entry(const Wt::Dbo::ptr<kanban_task_record>& p);
	static task_type_entry   to_entry(const Wt::Dbo::ptr<task_type_record>& p);
};
