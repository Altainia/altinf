#pragma once

#include "kanban.hpp"

#include <Wt/WContainerWidget.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

// Renders an interactive Kanban board via client-side JavaScript.
// Drag-and-drop column changes call on_move(task_id, new_status, new_sort_order).
// Edit-button clicks call on_edit(task_id).
class kanban_board_widget: public Wt::WContainerWidget
{
public:
	kanban_board_widget(std::vector<kanban_task_entry>                          tasks,
	                    bool                                                    can_edit,
	                    const std::map<long long, std::string>&                 type_colors,
	                    std::function<void(long long, const std::string&, int)> on_move,
	                    std::function<void(long long)>                          on_edit);

	void refresh(std::vector<kanban_task_entry>         tasks,
	             bool                                   can_edit,
	             const std::map<long long, std::string>& type_colors);

private:
	std::string                      m_mount_id;
	std::string                      m_cb_id;
	Wt::WContainerWidget*            m_mount{nullptr};
	std::map<long long, std::string> m_type_colors;

	std::string serialize_tasks(const std::vector<kanban_task_entry>& tasks) const;
	void        init_js(const std::string& json, bool can_edit);
};
