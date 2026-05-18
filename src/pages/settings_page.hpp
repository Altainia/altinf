#pragma once

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

class settings_page: public Wt::WContainerWidget
{
public:
	settings_page(org_db& odb, const session_data& session);
};
