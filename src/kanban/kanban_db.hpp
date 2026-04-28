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
	long long                 create_team(const std::string& name);
	void                      rename_team(long long id, const std::string& name);
	std::vector<team_entry>   all_teams();
	std::optional<team_entry> find_team(long long id);

	// Members
	void                     add_member(long long team_id, const std::string& username);
	void                     remove_member(long long team_id, const std::string& username);
	std::vector<std::string> members_for_team(long long team_id);
	bool                     is_member(long long team_id, const std::string& username);

	// Tasks
	long long                          add_task(const kanban_task_entry& e);
	void                               update_task(const kanban_task_entry& e);
	void                               update_task_status(long long id, const std::string& status, int sort_order);
	void                               delete_task(long long id);
	std::optional<kanban_task_entry>   find_task(long long id);
	std::vector<kanban_task_entry>     tasks_for_team(long long team_id);

	// Permission helpers
	bool can_view_board(long long team_id, const std::string& username, permission::flags perms);
	bool can_edit_board(long long team_id, const std::string& username, permission::flags perms);

private:
	Wt::Dbo::Session m_dbo;

	static team_entry        to_entry(const Wt::Dbo::ptr<team_record>& p);
	static kanban_task_entry to_entry(const Wt::Dbo::ptr<kanban_task_record>& p);
};
