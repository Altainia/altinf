#pragma once

#include "auth/session_data.h"
#include "gantt/gantt.h"

#include <Wt/WContainerWidget.h>

#include <vector>

class gantt_list_page: public Wt::WContainerWidget
{
public:
	gantt_list_page(const std::vector<gantt_project_entry>& projects,
	                const session_data&                     session);
};
