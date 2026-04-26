#pragma once

#include "gantt.hpp"

#include <Wt/Dbo/Session.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class gantt_db
{
public:
	explicit gantt_db(const std::string& db_path);

	// Projects
	long long                          create_project(const gantt_project_entry& e);
	void                               update_project(const gantt_project_entry& e);
	void                               delete_project(long long id);
	std::optional<gantt_project_entry> find_project(long long id);
	std::vector<gantt_project_entry>   projects_visible_to(const std::string& username,
	                                                       uint64_t           perms);

	// Tasks
	long long                     add_task(const gantt_task_entry& e);
	void                          update_task(const gantt_task_entry& e);
	void                          delete_task(long long id);
	void                          delete_tasks_for_project(long long project_id);
	std::vector<gantt_task_entry> tasks_for_project(long long project_id);

	// Viewers
	void                     add_viewer(long long project_id, const std::string& username);
	void                     remove_viewer(long long project_id, const std::string& username);
	void                     delete_viewers_for_project(long long project_id);
	std::vector<std::string> viewers_for_project(long long project_id);

	// Permission helpers
	bool can_view(long long project_id, const std::string& username, uint64_t perms);
	bool can_edit(long long project_id, const std::string& username, uint64_t perms);

private:
	static gantt_project_entry to_entry(const Wt::Dbo::ptr<gantt_project_record>& p);
	static gantt_task_entry    to_entry(const Wt::Dbo::ptr<gantt_task_record>& p);

	Wt::Dbo::Session m_dbo;
};
