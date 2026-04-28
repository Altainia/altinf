#pragma once

#include "auth/user_db.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WText.h>

#include <functional>
#include <optional>
#include <string>

class account_editor_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new user
	// existing != nullptr  ->  edit user
	account_editor_page(user_db*              db,
	                    const user_entry*     existing,
	                    std::function<void()> on_save);

private:
	user_db*                  m_db;
	std::optional<user_entry> m_existing;
	std::function<void()>     m_on_save;
	Wt::WLineEdit*            m_username{nullptr};
	Wt::WLineEdit*            m_display_name{nullptr};
	Wt::WPasswordEdit*        m_password{nullptr};
	Wt::WPasswordEdit*        m_password_confirm{nullptr};
	Wt::WCheckBox*            m_perm_admin{nullptr};
	Wt::WCheckBox*            m_perm_manage_users{nullptr};
	Wt::WCheckBox*            m_perm_post_write{nullptr};
	Wt::WCheckBox*            m_perm_org_create{nullptr};
	Wt::WText*                m_status{nullptr};
	Wt::WContainerWidget*     m_tokens_container{nullptr};

	void save();
	void build_token_list();
	void generate_token();
};
