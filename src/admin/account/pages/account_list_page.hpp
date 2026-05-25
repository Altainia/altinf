#pragma once

#include <Wt/WContainerWidget.h>

#include <functional>
#include <memory>
#include <string>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

class account_list_page: public Wt::WContainerWidget
{
public:
	account_list_page(user_db&                                db,
	                  const session_data&                     session,
	                  std::function<void(const std::string&)> on_delete);

	~account_list_page() override;

private:
	user_db&                                m_db;
	const session_data&                     m_session;
	std::function<void(const std::string&)> m_on_delete;
	std::string                             m_session_id;
	std::shared_ptr<bool>                   m_alive{std::make_shared<bool>(true)};

	void render();
	void refresh();
};