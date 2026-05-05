#include "kanban_db.hpp"

#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>

#include "auth/permission.hpp"

kanban_db::kanban_db(const std::string& db_path)
{
	m_dbo.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
	m_dbo.mapClass<team_record>("team");
	m_dbo.mapClass<team_member_record>("team_member");
	m_dbo.mapClass<kanban_task_record>("kanban_task");

	try
	{
		m_dbo.createTables();
	}
	catch(const Wt::Dbo::Exception&)
	{
		// Tables already exist. Add new columns if they are missing (idempotent).
	}

	// Ensure new columns exist on pre-existing databases.
	// Each ALTER TABLE needs its own transaction; SQLite will error (caught) if
	// the column already exists, which is the idempotent-migration behavior we want.
	try
	{
		Wt::Dbo::Transaction t{m_dbo};
		m_dbo.execute("ALTER TABLE team ADD COLUMN org_id INTEGER NOT NULL DEFAULT 0");
	}
	catch(...)
	{}
	try
	{
		Wt::Dbo::Transaction t{m_dbo};
		m_dbo.execute("ALTER TABLE team_member ADD COLUMN is_lead INTEGER NOT NULL DEFAULT 0");
	}
	catch(...)
	{}
}

// ---- static helpers ----

team_entry kanban_db::to_entry(const Wt::Dbo::ptr<team_record>& p)
{
	return {.id = p.id(), .name = p->name, .org_id = p->org_id};
}

kanban_task_entry kanban_db::to_entry(const Wt::Dbo::ptr<kanban_task_record>& p)
{
	kanban_task_entry e;
	e.id          = p.id();
	e.team_id     = p->team_id;
	e.status      = p->status;
	e.title       = p->title;
	e.description = p->description;
	e.assigned_to = p->assigned_to;
	e.start_date  = p->start_date;
	e.end_date    = p->end_date;
	e.type_id     = p->type_id;
	e.sort_order  = p->sort_order;
	return e;
}

// ---- Teams ----

long long kanban_db::create_team(const std::string& name, long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p = m_dbo.add(std::make_unique<team_record>());
	p.modify()->name       = name;
	p.modify()->org_id     = org_id;
	m_dbo.flush();
	return p.id();
}

void kanban_db::rename_team(long long id, const std::string& name)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
	if(!results.empty())
	{
		(*results.begin()).modify()->name = name;
	}
}

void kanban_db::set_team_org(long long team_id, long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().where("id = ?").bind(team_id).resultList();
	if(!results.empty())
	{
		(*results.begin()).modify()->org_id = org_id;
	}
}

std::vector<team_entry> kanban_db::all_teams()
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().orderBy("id").resultList();
	std::vector<team_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

std::vector<team_entry> kanban_db::teams_for_org(long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().where("org_id = ?").bind(org_id).orderBy("id").resultList();
	std::vector<team_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

std::optional<team_entry> kanban_db::find_team(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	return to_entry(*results.begin());
}

void kanban_db::delete_team(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};

	// Remove all members and tasks first.
	{
		const auto rows = m_dbo.find<team_member_record>()
		                    .where("team_id = ?")
		                    .bind(id)
		                    .resultList();
		for(auto p: rows)
		{
			p.remove();
		}
	}
	{
		const auto rows = m_dbo.find<kanban_task_record>()
		                    .where("team_id = ?")
		                    .bind(id)
		                    .resultList();
		for(auto p: rows)
		{
			p.remove();
		}
	}
	{
		const auto rows =
		  m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
		if(!rows.empty())
		{
			(*rows.begin()).remove();
		}
	}
}

// ---- Members ----

void kanban_db::add_member(long long team_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	if(is_member(team_id, username))
	{
		return;
	}
	auto p               = m_dbo.add(std::make_unique<team_member_record>());
	p.modify()->team_id  = team_id;
	p.modify()->username = username;
	p.modify()->is_lead  = 0;
}

void kanban_db::remove_member(long long team_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_member_record>()
	                       .where("team_id = ? AND username = ?")
	                       .bind(team_id)
	                       .bind(username)
	                       .resultList();
	if(!results.empty())
	{
		(*results.begin()).remove();
	}
}

void kanban_db::remove_member_from_org_teams(long long org_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           teams =
	  m_dbo.find<team_record>().where("org_id = ?").bind(org_id).resultList();
	for(const auto& team: teams)
	{
		const auto rows = m_dbo.find<team_member_record>()
		                    .where("team_id = ? AND username = ?")
		                    .bind(team.id())
		                    .bind(username)
		                    .resultList();
		if(!rows.empty())
		{
			(*rows.begin()).remove();
		}
	}
}

void kanban_db::remove_member_from_all_teams(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           rows = m_dbo.find<team_member_record>()
	                    .where("username = ?")
	                    .bind(username)
	                    .resultList();
	for(const auto& r: rows)
	{
		Wt::Dbo::ptr<team_member_record> row = r;
		row.remove();
	}
}

void kanban_db::set_team_lead(long long team_id, const std::string& username, bool is_lead)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_member_record>()
	                       .where("team_id = ? AND username = ?")
	                       .bind(team_id)
	                       .bind(username)
	                       .resultList();
	if(!results.empty())
	{
		(*results.begin()).modify()->is_lead = is_lead ? 1 : 0;
	}
}

bool kanban_db::is_team_lead(long long team_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_member_record>()
	                       .where("team_id = ? AND username = ? AND is_lead = 1")
	                       .bind(team_id)
	                       .bind(username)
	                       .resultList();
	return !results.empty();
}

std::vector<std::string> kanban_db::members_for_team(long long team_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_member_record>()
	                       .where("team_id = ?")
	                       .bind(team_id)
	                       .orderBy("username")
	                       .resultList();
	std::vector<std::string> out;
	for(const auto& p: results)
	{
		out.push_back(p->username);
	}
	return out;
}

std::vector<team_member_entry> kanban_db::team_member_entries(long long team_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_member_record>()
	                       .where("team_id = ?")
	                       .bind(team_id)
	                       .orderBy("username")
	                       .resultList();
	std::vector<team_member_entry> out;
	for(const auto& p: results)
	{
		out.push_back({.username = p->username, .is_lead = p->is_lead != 0});
	}
	return out;
}

bool kanban_db::is_member(long long team_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_member_record>()
	                       .where("team_id = ? AND username = ?")
	                       .bind(team_id)
	                       .bind(username)
	                       .resultList();
	return !results.empty();
}

std::vector<long long> kanban_db::team_ids_for_user(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_member_record>()
	                       .where("username = ?")
	                       .bind(username)
	                       .resultList();
	std::vector<long long> out;
	for(const auto& r: results)
	{
		out.push_back(r->team_id);
	}
	return out;
}

// ---- Tasks ----

long long kanban_db::add_task(const kanban_task_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p  = m_dbo.add(std::make_unique<kanban_task_record>());
	p.modify()->team_id     = e.team_id;
	p.modify()->status      = e.status.empty() ? "todo" : e.status;
	p.modify()->title       = e.title;
	p.modify()->description = e.description;
	p.modify()->assigned_to = e.assigned_to;
	p.modify()->start_date  = e.start_date;
	p.modify()->end_date    = e.end_date;
	p.modify()->type_id     = e.type_id;
	p.modify()->sort_order  = e.sort_order;
	m_dbo.flush();
	return p.id();
}

void kanban_db::update_task(const kanban_task_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(e.id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
	p.modify()->status                 = e.status;
	p.modify()->title                  = e.title;
	p.modify()->description            = e.description;
	p.modify()->assigned_to            = e.assigned_to;
	p.modify()->start_date             = e.start_date;
	p.modify()->end_date               = e.end_date;
	p.modify()->type_id                = e.type_id;
	p.modify()->sort_order             = e.sort_order;
}

void kanban_db::update_task_status(long long id, const std::string& status, int sort_order)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
	p.modify()->status                 = status;
	p.modify()->sort_order             = sort_order;
}

bool kanban_db::self_assign(long long task_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(task_id).resultList();
	if(results.empty())
	{
		return false;
	}
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
	if(!p->assigned_to.empty())
	{
		return false; // Already assigned — self-assign not permitted.
	}
	if(!is_member(p->team_id, username))
	{
		return false;
	}
	p.modify()->assigned_to = username;
	return true;
}

void kanban_db::delete_task(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(!results.empty())
	{
		(*results.begin()).remove();
	}
}

std::optional<kanban_task_entry> kanban_db::find_task(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	return to_entry(*results.begin());
}

std::vector<kanban_task_entry> kanban_db::tasks_for_team(long long team_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<kanban_task_record>()
	                       .where("team_id = ?")
	                       .bind(team_id)
	                       .orderBy("sort_order, id")
	                       .resultList();
	std::vector<kanban_task_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

// ---- Permission helpers ----

bool kanban_db::can_view_board(long long          team_id,
                               const std::string& username,
                               permission::flags  perms,
                               bool               is_org_lead)
{
	if(perms.has_any(permission::admin) || is_org_lead)
	{
		return true;
	}
	return is_member(team_id, username);
}

bool kanban_db::can_edit_board(long long          team_id,
                               const std::string& username,
                               permission::flags  perms,
                               bool               is_org_lead,
                               bool               is_team_lead)
{
	if(perms.has_any(permission::admin) || is_org_lead || is_team_lead)
	{
		return true;
	}
	return false;
}
