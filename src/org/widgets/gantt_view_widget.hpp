#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "org/kanban.hpp"

// Read-only Gantt timeline rendered client-side as an SVG.
// Only tasks with valid start_date and end_date are shown.
class gantt_view_widget: public Wt::WContainerWidget
{
public:
	explicit gantt_view_widget(std::vector<kanban_task_entry>          tasks,
	                           const std::map<long long, std::string>& type_colors,
	                           std::function<void(long long)>          on_edit = {});

	void refresh(std::vector<kanban_task_entry>          tasks,
	             const std::map<long long, std::string>& type_colors);

private:
	std::map<long long, std::string> m_type_colors;
	std::string                      m_mount_id;
	std::string                      m_cb_id;
	std::function<void(long long)>   m_on_edit;

	std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
};