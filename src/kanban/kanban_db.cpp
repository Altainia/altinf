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
		// Tables already exist — ignore
	}

	// Ensure at least one default team exists
	Wt::Dbo::Transaction t{m_dbo};
	if(m_dbo.find<team_record>().resultList().empty())
	{
		auto p           = m_dbo.add(std::make_unique<team_record>());
		p.modify()->name = "Team";
	}
}

// ---- static helpers ----

team_entry kanban_db::to_entry(const Wt::Dbo::ptr<team_record>& p)
{
	return {.id = p.id(), .name = p->name};
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
	e.color       = p->color;
	e.sort_order  = p->sort_order;
	return e;
}

// ---- Teams ----

long long kanban_db::create_team(const std::string& name)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p = m_dbo.add(std::make_unique<team_record>());
	p.modify()->name       = name;
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
		Wt::Dbo::ptr<team_record> p = *results.begin();
		p.modify()->name            = name;
	}
}

std::vector<team_entry> kanban_db::all_teams()
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().orderBy("id").resultList();
	std::vector<team_entry> out;
	for(const auto& p: results)
		out.push_back(to_entry(p));
	return out;
}

std::optional<team_entry> kanban_db::find_team(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
		return std::nullopt;
	return to_entry(*results.begin());
}

// ---- Members ----

void kanban_db::add_member(long long team_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	if(is_member(team_id, username))
		return;

	auto p               = m_dbo.add(std::make_unique<team_member_record>());
	p.modify()->team_id  = team_id;
	p.modify()->username = username;
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
		Wt::Dbo::ptr<team_member_record> p = *results.begin();
		p.remove();
	}
}

std::vector<std::string> kanban_db::members_for_team(long long team_id)
{
	Wt::Dbo::Transaction     t{m_dbo};
	const auto               results = m_dbo.find<team_member_record>()
	                                     .where("team_id = ?")
	                                     .bind(team_id)
	                                     .resultList();
	std::vector<std::string> out;
	for(const auto& p: results)
		out.push_back(p->username);
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
	p.modify()->color       = e.color;
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
		return;
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
	p.modify()->status                 = e.status;
	p.modify()->title                  = e.title;
	p.modify()->description            = e.description;
	p.modify()->assigned_to            = e.assigned_to;
	p.modify()->start_date             = e.start_date;
	p.modify()->end_date               = e.end_date;
	p.modify()->color                  = e.color;
	p.modify()->sort_order             = e.sort_order;
}

void kanban_db::update_task_status(long long id, const std::string& status, int sort_order)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
		return;
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
	p.modify()->status                 = status;
	p.modify()->sort_order             = sort_order;
}

void kanban_db::delete_task(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(!results.empty())
	{
		Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
		p.remove();
	}
}

std::optional<kanban_task_entry> kanban_db::find_task(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
		return std::nullopt;
	return to_entry(*results.begin());
}

std::vector<kanban_task_entry> kanban_db::tasks_for_team(long long team_id)
{
	Wt::Dbo::Transaction           t{m_dbo};
	const auto                     results = m_dbo.find<kanban_task_record>()
	                                           .where("team_id = ?")
	                                           .bind(team_id)
	                                           .orderBy("sort_order, id")
	                                           .resultList();
	std::vector<kanban_task_entry> out;
	for(const auto& p: results)
		out.push_back(to_entry(p));
	return out;
}

// ---- Permission helpers ----

bool kanban_db::can_view_board(long long team_id, const std::string& username, permission::flags perms)
{
	if(perms.has_any(permission::admin))
		return true;
	return is_member(team_id, username);
}

bool kanban_db::can_edit_board(long long team_id, const std::string& username, permission::flags perms)
{
	if(perms.has_any(permission::admin))
		return true;
	if(!perms.has_any(permission::gantt_write))
		return false;
	return is_member(team_id, username);
}
