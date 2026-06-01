#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WSignal.h>

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
	                           const std::map<long long, std::string>& type_colors);

	void refresh(std::vector<kanban_task_entry>          tasks,
	             const std::map<long long, std::string>& type_colors);

	Wt::Signal<long long> edit_requested;
	// Emitted on mobile, where tapping an agenda item navigates to the full-page
	// editor instead of opening the popup dialog.
	Wt::Signal<long long> nav_requested;

private:
	std::map<long long, std::string> m_type_colors;
	std::string                      m_mount_id;
	std::string                      m_cb_id;

	std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
};
