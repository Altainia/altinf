#include "org_db.hpp"

#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <Wt/WDateTime.h>

org_db::org_db(const std::string& db_path)
{
	m_dbo.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
	m_dbo.mapClass<org_record>("organization");
	m_dbo.mapClass<org_member_record>("org_member");
	m_dbo.mapClass<notification_record>("notification");
	m_dbo.mapClass<user_pref_record>("user_pref");

	try
	{
		m_dbo.createTables();
	}
	catch(const Wt::Dbo::Exception&)
	{
		// Tables already exist — ignore.
	}
}

// ---- static helpers ----

org_entry org_db::to_entry(const Wt::Dbo::ptr<org_record>& p)
{
	return {.id = p.id(), .name = p->name};
}

org_member_entry org_db::to_entry(const Wt::Dbo::ptr<org_member_record>& p)
{
	return {
	  .id       = p.id(),
	  .org_id   = p->org_id,
	  .username = p->username,
	  .is_lead  = p->is_lead != 0,
	  .status   = p->status,
	};
}

notification_entry org_db::to_entry(const Wt::Dbo::ptr<notification_record>& p)
{
	return {
	  .id         = p.id(),
	  .username   = p->username,
	  .type       = p->type,
	  .payload    = p->payload,
	  .is_read    = p->is_read != 0,
	  .created_at = p->created_at,
	};
}

// ---- Organizations ----

long long org_db::create_org(const std::string& name, const std::string& creator_username)
{
	Wt::Dbo::Transaction t{m_dbo};

	auto p           = m_dbo.add(std::make_unique<org_record>());
	p.modify()->name = name;
	m_dbo.flush();
	const long long org_id = p.id();

	// Creator becomes an active lead immediately — no invite needed.
	auto m               = m_dbo.add(std::make_unique<org_member_record>());
	m.modify()->org_id   = org_id;
	m.modify()->username = creator_username;
	m.modify()->is_lead  = 1;
	m.modify()->status   = "active";

	return org_id;
}

std::optional<org_entry> org_db::find_org(long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<org_record>().where("id = ?").bind(org_id).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	return to_entry(*results.begin());
}

std::vector<org_entry> org_db::all_orgs()
{
	Wt::Dbo::Transaction   t{m_dbo};
	const auto             results = m_dbo.find<org_record>().orderBy("id").resultList();
	std::vector<org_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

// ---- Membership ----

int org_db::active_lead_count(long long org_id)
{
	// Called inside an existing transaction.
	const auto results = m_dbo.find<org_member_record>()
	                       .where("org_id = ? AND is_lead = 1 AND status = 'active'")
	                       .bind(org_id)
	                       .resultList();
	return static_cast<int>(results.size());
}

void org_db::invite_to_org(long long org_id, const std::string& username, bool as_lead)
{
	Wt::Dbo::Transaction t{m_dbo};

	// If a row already exists (pending or declined), reset it and re-notify.
	const auto existing = m_dbo.find<org_member_record>()
	                        .where("org_id = ? AND username = ?")
	                        .bind(org_id)
	                        .bind(username)
	                        .resultList();

	if(!existing.empty())
	{
		Wt::Dbo::ptr<org_member_record> row = *existing.begin();
		if(row->status == "active")
		{
			return; // Already a member — nothing to do.
		}
		row.modify()->status  = "pending";
		row.modify()->is_lead = as_lead ? 1 : 0;
	}
	else
	{
		auto row               = m_dbo.add(std::make_unique<org_member_record>());
		row.modify()->org_id   = org_id;
		row.modify()->username = username;
		row.modify()->is_lead  = as_lead ? 1 : 0;
		row.modify()->status   = "pending";
	}

	// Look up org name for the notification payload.
	const auto org_results =
	  m_dbo.find<org_record>().where("id = ?").bind(org_id).resultList();
	const std::string org_name =
	  org_results.empty() ? "" : (*org_results.begin())->name;

	auto n               = m_dbo.add(std::make_unique<notification_record>());
	n.modify()->username = username;
	n.modify()->type     = "org_invite";
	n.modify()->payload  = make_org_invite_payload(org_id, org_name);
	n.modify()->is_read  = 0;
	n.modify()->created_at =
	  Wt::WDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toUTF8();
}

void org_db::accept_invite(long long org_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<org_member_record>()
	                       .where("org_id = ? AND username = ? AND status = 'pending'")
	                       .bind(org_id)
	                       .bind(username)
	                       .resultList();
	if(results.empty())
	{
		return;
	}
	(*results.begin()).modify()->status = "active";
}

void org_db::decline_invite(long long org_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<org_member_record>()
	                       .where("org_id = ? AND username = ? AND status = 'pending'")
	                       .bind(org_id)
	                       .bind(username)
	                       .resultList();
	if(results.empty())
	{
		return;
	}
	(*results.begin()).modify()->status = "declined";
}

bool org_db::remove_org_member(long long org_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<org_member_record>()
	                       .where("org_id = ? AND username = ?")
	                       .bind(org_id)
	                       .bind(username)
	                       .resultList();
	if(results.empty())
	{
		return true;
	}

	Wt::Dbo::ptr<org_member_record> row = *results.begin();

	// Block removal if this is the last active lead.
	if(row->is_lead && row->status == "active" && active_lead_count(org_id) <= 1)
	{
		return false;
	}

	row.remove();
	return true;
}

void org_db::rescind_invite_notification(long long org_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto orgs =
	  m_dbo.find<org_record>().where("id = ?").bind(org_id).resultList();
	if(orgs.empty())
	{
		return;
	}
	const std::string org_name = (*orgs.begin())->name;

	// Find the unread org_invite notification for this user+org and rewrite payload.
	const auto notifs = m_dbo.find<notification_record>()
	                      .where("username = ? AND type = 'org_invite' AND is_read = 0")
	                      .bind(username)
	                      .resultList();
	for(const auto& n: notifs)
	{
		if(json_long(n->payload, "org_id") == org_id)
		{
			Wt::Dbo::ptr<notification_record> row = n;
			row.modify()->payload                 = make_org_invite_rescinded_payload(org_id, org_name);
			break;
		}
	}
}

void org_db::remove_user_from_all_orgs(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto memberships = m_dbo.find<org_member_record>()
	                           .where("username = ?")
	                           .bind(username)
	                           .resultList();
	for(const auto& r: memberships)
	{
		Wt::Dbo::ptr<org_member_record> row = r;
		row.remove();
	}

	const auto notifs = m_dbo.find<notification_record>()
	                      .where("username = ?")
	                      .bind(username)
	                      .resultList();
	for(const auto& r: notifs)
	{
		Wt::Dbo::ptr<notification_record> row = r;
		row.remove();
	}
}

bool org_db::set_org_lead(long long org_id, const std::string& username, bool is_lead)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<org_member_record>()
	                       .where("org_id = ? AND username = ? AND status = 'active'")
	                       .bind(org_id)
	                       .bind(username)
	                       .resultList();
	if(results.empty())
	{
		return true;
	}

	Wt::Dbo::ptr<org_member_record> row = *results.begin();

	if(!is_lead && active_lead_count(org_id) <= 1)
	{
		return false; // Would leave 0 leads.
	}

	row.modify()->is_lead = is_lead ? 1 : 0;
	return true;
}

bool org_db::is_org_lead(long long org_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<org_member_record>()
	    .where("org_id = ? AND username = ? AND is_lead = 1 AND status = 'active'")
	    .bind(org_id)
	    .bind(username)
	    .resultList();
	return !results.empty();
}

bool org_db::is_org_member(long long org_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<org_member_record>()
	    .where("org_id = ? AND username = ? AND status = 'active'")
	    .bind(org_id)
	    .bind(username)
	    .resultList();
	return !results.empty();
}

std::vector<org_member_entry> org_db::org_members(long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<org_member_record>()
	    .where("org_id = ? AND status = 'active'")
	    .bind(org_id)
	    .orderBy("username")
	    .resultList();
	std::vector<org_member_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

std::vector<org_member_entry> org_db::org_pending(long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<org_member_record>()
	    .where("org_id = ? AND status = 'pending'")
	    .bind(org_id)
	    .orderBy("username")
	    .resultList();
	std::vector<org_member_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

std::vector<org_entry> org_db::orgs_for_user(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           memberships =
	  m_dbo.find<org_member_record>()
	    .where("username = ? AND status = 'active'")
	    .bind(username)
	    .resultList();

	std::vector<org_entry> out;
	for(const auto& m: memberships)
	{
		const auto orgs =
		  m_dbo.find<org_record>().where("id = ?").bind(m->org_id).resultList();
		if(!orgs.empty())
		{
			out.push_back(to_entry(*orgs.begin()));
		}
	}
	return out;
}

// ---- Notifications ----

void org_db::push_notification(const std::string& username,
                               const std::string& type,
                               const std::string& payload_json)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 n = m_dbo.add(std::make_unique<notification_record>());
	n.modify()->username   = username;
	n.modify()->type       = type;
	n.modify()->payload    = payload_json;
	n.modify()->is_read    = 0;
	n.modify()->created_at =
	  Wt::WDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toUTF8();
}

void org_db::mark_read(long long notification_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<notification_record>().where("id = ?").bind(notification_id).resultList();
	if(!results.empty())
	{
		(*results.begin()).modify()->is_read = 1;
	}
}

int org_db::unread_count(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<notification_record>()
	    .where("username = ? AND is_read = 0")
	    .bind(username)
	    .resultList();
	return static_cast<int>(results.size());
}

std::vector<notification_entry> org_db::notifications_for_user(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<notification_record>()
	    .where("username = ?")
	    .bind(username)
	    .orderBy("id DESC")
	    .resultList();
	std::vector<notification_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

// ---- User preferences ----

void org_db::set_last_org(const std::string& username, long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<user_pref_record>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		auto p                  = m_dbo.add(std::make_unique<user_pref_record>());
		p.modify()->username    = username;
		p.modify()->last_org_id = org_id;
	}
	else
	{
		(*results.begin()).modify()->last_org_id = org_id;
	}
}

std::optional<long long> org_db::get_last_org(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<user_pref_record>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	const long long v = (*results.begin())->last_org_id;
	return v == 0 ? std::nullopt : std::optional<long long>{v};
}
