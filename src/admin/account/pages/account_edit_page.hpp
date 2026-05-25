#pragma once

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>

#include <optional>
#include <string>

#include "auth/user_db.hpp"

class account_edit_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new user
	// existing != nullptr  ->  edit user
	account_edit_page(user_db* db, const user_entry* existing);

	Wt::Signal<> saved;

private:
	user_db*                  m_db;
	std::optional<user_entry> m_existing;
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
