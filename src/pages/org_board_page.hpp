#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WContainerWidget.h>

#include <map>
#include <memory>
#include <string>

class org_board_page: public Wt::WContainerWidget
{
public:
	org_board_page(org_db&             odb,
	               kanban_db&          kdb,
	               long long           org_id,
	               const session_data& session);

	~org_board_page() override;

private:
	kanban_db&                       m_db;
	long long                        m_org_id{0};
	std::string                      m_session_id;
	std::shared_ptr<bool>            m_alive{std::make_shared<bool>(true)};
	std::map<long long, std::string> m_type_colors;
};
