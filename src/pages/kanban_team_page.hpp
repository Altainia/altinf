#pragma once

#include "auth/session_data.hpp"
#include "auth/user_db.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

// Org management page — org leads reach this at /org/{id}/manage.
class kanban_team_page: public Wt::WContainerWidget
{
public:
	kanban_team_page(org_db&             odb,
	                 kanban_db&          kdb,
	                 user_db&            udb,
	                 long long           org_id,
	                 const session_data& session);

private:
	org_db&               m_odb;
	kanban_db&            m_kdb;
	user_db&              m_udb;
	long long             m_org_id;
	const session_data&   m_session;

	Wt::WContainerWidget* m_members_section{nullptr};
	Wt::WContainerWidget* m_pending_section{nullptr};
	Wt::WLineEdit*        m_invite_input{nullptr};
	Wt::WCheckBox*        m_invite_lead{nullptr};
	Wt::WText*            m_invite_msg{nullptr};
	Wt::WContainerWidget* m_teams_section{nullptr};
	Wt::WLineEdit*        m_new_team_input{nullptr};

	void refresh_members();
	void refresh_pending();
	void refresh_teams();
	void build_team_block(Wt::WContainerWidget* parent, const team_entry& team);
};
