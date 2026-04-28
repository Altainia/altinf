#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

class org_landing_page: public Wt::WContainerWidget
{
public:
	org_landing_page(org_db&             odb,
	                 kanban_db&          kdb,
	                 long long           org_id,
	                 const session_data& session,
	                 bool                is_org_lead);
};
