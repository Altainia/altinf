#pragma once

#include "auth/session_data.h"
#include "gantt/gantt.h"

#include <Wt/WContainerWidget.h>

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

	static int         date_to_days(const std::string& d);
	static std::string days_to_date_label(int days);

	Wt::WContainerWidget* m_chart_wrap{nullptr};
};
