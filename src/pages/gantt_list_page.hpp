#pragma once

#include "auth/session_data.hpp"
#include "gantt/gantt.hpp"

#include <Wt/WContainerWidget.h>

#include <vector>

class gantt_list_page: public Wt::WContainerWidget
{
public:
	gantt_list_page(const std::vector<gantt_project_entry>& projects,
	                const session_data&                     session);
};
