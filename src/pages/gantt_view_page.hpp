#pragma once

#include "auth/session_data.hpp"
#include "gantt/gantt.hpp"

#include <Wt/WContainerWidget.h>
#include <Wt/WDate.h>

#include <functional>
#include <vector>

class gantt_view_page: public Wt::WContainerWidget
{
public:
	gantt_view_page(const gantt_project_entry&           project,
	                const std::vector<gantt_task_entry>& tasks,
	                const session_data&                  session,
	                std::function<void()>                on_edit);

private:
	void render_chart(const std::vector<gantt_task_entry>& tasks);

	static std::string date_label(const Wt::WDate& d);

	Wt::WContainerWidget* m_chart_wrap{nullptr};
};
