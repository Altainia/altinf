#pragma once

#include "kanban.hpp"

#include <Wt/WContainerWidget.h>

#include <map>
#include <vector>

// Read-only Gantt timeline rendered client-side as an SVG.
// Only tasks with valid start_date and end_date are shown.
class gantt_view_widget: public Wt::WContainerWidget
{
public:
	explicit gantt_view_widget(std::vector<kanban_task_entry>   tasks,
	                           std::map<long long, std::string> type_colors);

	void refresh(std::vector<kanban_task_entry>   tasks,
	             std::map<long long, std::string> type_colors);

private:
	std::map<long long, std::string> m_type_colors;

	std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
};
