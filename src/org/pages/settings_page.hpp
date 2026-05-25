#pragma once

#include <Wt/WContainerWidget.h>

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

class settings_page: public Wt::WContainerWidget
{
public:
	settings_page(org_db& odb, const session_data& session);
};