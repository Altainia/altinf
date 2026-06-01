#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>

#include <map>
#include <string>
#include <vector>

#include "org/kanban.hpp"

// Renders an interactive Kanban board via client-side JavaScript.
// Drag-and-drop column changes emit moved(task_id, new_status, new_sort_order).
// Edit-button clicks emit edit_requested(task_id).
class kanban_board_widget: public Wt::WContainerWidget
{
public:
	kanban_board_widget(std::vector<kanban_task_entry>          tasks,
	                    bool                                    can_move_columns,
	                    bool                                    can_move_done,
	                    const std::map<long long, std::string>& type_colors);

	void refresh(std::vector<kanban_task_entry>          tasks,
	             bool                                    can_move_columns,
	             bool                                    can_move_done,
	             const std::map<long long, std::string>& type_colors);

	Wt::Signal<long long, std::string, int> moved;
	Wt::Signal<long long>                   edit_requested;
	// Emitted on mobile, where a card tap navigates to the full-page editor
	// instead of opening the popup dialog.
	Wt::Signal<long long> nav_requested;

private:
	std::string                      m_mount_id;
	std::string                      m_cb_id;
	Wt::WContainerWidget*            m_mount{nullptr};
	std::map<long long, std::string> m_type_colors;

	std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
	void        init_js(const std::string& json, bool can_move_columns, bool can_move_done);
};
