#pragma once

#include "kanban.hpp"

#include <Wt/WContainerWidget.h>

#include <vector>

// Read-only Gantt timeline rendered client-side as an SVG.
// Only tasks with valid start_date and end_date are shown.
class gantt_view_widget: public Wt::WContainerWidget
{
public:
	explicit gantt_view_widget(std::vector<kanban_task_entry> tasks);

	void refresh(std::vector<kanban_task_entry> tasks);

private:
	static std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks);
};
