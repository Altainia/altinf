#pragma once

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>

#include <optional>
#include <string>

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"

class account_edit_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new user
	// existing != nullptr  ->  edit user
	// `session` is the acting admin, used for audit attribution and to gate
	// history/Google controls.
	account_edit_page(user_db* db, const session_data& session, const user_entry* existing);

	Wt::Signal<> saved;

private:
	user_db*                  m_db;
	session_data              m_session;
	std::optional<user_entry> m_existing;
	Wt::WLineEdit*            m_username{nullptr};
	Wt::WLineEdit*            m_display_name{nullptr};
	Wt::WPasswordEdit*        m_password{nullptr};
	Wt::WPasswordEdit*        m_password_confirm{nullptr};
	Wt::WCheckBox*            m_perm_admin{nullptr};
	Wt::WCheckBox*            m_perm_manage_users{nullptr};
	Wt::WCheckBox*            m_perm_post_write{nullptr};
	Wt::WCheckBox*            m_perm_org_create{nullptr};
	Wt::WCheckBox*            m_perm_api_token{nullptr};
	Wt::WCheckBox*            m_perm_view_history{nullptr};
	Wt::WText*                m_status{nullptr};

	void save();
};
