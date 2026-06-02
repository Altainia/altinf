#pragma once

#include <Wt/WContainerWidget.h>

#include <string>

#include "auth/user_db.hpp"

// Reusable API-token manager: lists a user's tokens by name with rename/revoke,
// and a "Generate" action that prompts for a name and reveals the raw token
// once. Used both by a user managing their own tokens (account_page) and by an
// admin managing another user's tokens (account_edit_page). All mutations are
// attributed to `actor_id` in the audit trail.
class token_manager_widget: public Wt::WContainerWidget
{
public:
	token_manager_widget(user_db& db, std::string username, long long actor_id);

private:
	user_db&    m_db;
	std::string m_username;
	long long   m_actor_id;

	Wt::WContainerWidget* m_list{nullptr};

	void build_list();
	void generate();
	void rename(long long token_id, const std::string& current_name);
	void revoke(long long token_id);
};
