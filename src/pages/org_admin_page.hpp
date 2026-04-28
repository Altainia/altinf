#pragma once

#include "auth/session_data.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

class org_admin_page: public Wt::WContainerWidget
{
public:
	org_admin_page(org_db& odb, const session_data& session);

private:
	org_db&            m_db;
	std::string        m_creator;
	Wt::WLineEdit*     m_name_input{nullptr};
	Wt::WText*         m_status_msg{nullptr};
	Wt::WContainerWidget* m_list{nullptr};

	void create_org();
	void refresh_list();
};
