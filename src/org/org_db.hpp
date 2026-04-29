#pragma once

#include "org.hpp"

#include <Wt/Dbo/Session.h>

#include <optional>
#include <string>
#include <vector>

class org_db
{
public:
	explicit org_db(const std::string& db_path);

	// Organizations
	long long                  create_org(const std::string& name,
	                                      const std::string& creator_username);
	std::optional<org_entry>   find_org(long long org_id);
	std::vector<org_entry>     all_orgs();

	// Membership
	void invite_to_org(long long org_id, const std::string& username, bool as_lead);
	void accept_invite(long long org_id, const std::string& username);
	void decline_invite(long long org_id, const std::string& username);
	// Returns false if removal would leave the org with 0 leads.
	bool remove_org_member(long long org_id, const std::string& username);
	// Unconditionally removes all org and notification rows for a deleted user.
	void remove_user_from_all_orgs(const std::string& username);
	// Returns false if demotion would leave the org with 0 leads.
	bool set_org_lead(long long org_id, const std::string& username, bool is_lead);

	bool                           is_org_lead(long long org_id, const std::string& username);
	bool                           is_org_member(long long org_id, const std::string& username);
	std::vector<org_member_entry>  org_members(long long org_id);   // active only
	std::vector<org_member_entry>  org_pending(long long org_id);   // pending only
	std::vector<org_entry>         orgs_for_user(const std::string& username); // active only

	// Notifications
	void push_notification(const std::string& username,
	                        const std::string& type,
	                        const std::string& payload_json);
	void mark_read(long long notification_id);
	int  unread_count(const std::string& username);
	std::vector<notification_entry> notifications_for_user(const std::string& username);

	// User preferences
	void                     set_last_org(const std::string& username, long long org_id);
	std::optional<long long> get_last_org(const std::string& username);

private:
	Wt::Dbo::Session m_dbo;

	int active_lead_count(long long org_id);

	static org_entry          to_entry(const Wt::Dbo::ptr<org_record>&);
	static org_member_entry   to_entry(const Wt::Dbo::ptr<org_member_record>&);
	static notification_entry to_entry(const Wt::Dbo::ptr<notification_record>&);
};
