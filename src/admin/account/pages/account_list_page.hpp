#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>

#include <memory>
#include <string>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

class account_list_page: public Wt::WContainerWidget
{
public:
	account_list_page(user_db& db, const session_data& session);

	~account_list_page() override;

	Wt::Signal<std::string> deleted;

private:
	user_db&              m_db;
	const session_data&   m_session;
	std::string           m_session_id;
	std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

	void render();
	void refresh();
};
