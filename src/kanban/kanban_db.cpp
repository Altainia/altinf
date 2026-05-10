#include "kanban_db.hpp"

#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <Wt/WDateTime.h>

#include "auth/permission.hpp"

kanban_db::kanban_db(const std::string& db_path)
{
	m_dbo.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
	m_dbo.mapClass<team_record>("team");
	m_dbo.mapClass<team_member_record>("team_member");
	m_dbo.mapClass<kanban_task_record>("kanban_task");
	m_dbo.mapClass<task_type_record>("task_type");
	m_dbo.mapClass<task_event_record>("task_event");
	m_dbo.mapClass<task_field_change_record>("task_field_change");
	m_dbo.mapClass<task_comment_record>("task_comment");
	m_dbo.mapClass<task_comment_event_record>("task_comment_event");

	try
	{
		m_dbo.createTables();
	}
	catch(const Wt::Dbo::Exception&)
	{}

	// Idempotent column migrations.
	auto migrate = [this](const std::string& sql) {
		try
		{
			Wt::Dbo::Transaction t{m_dbo};
			m_dbo.execute(sql);
		}
		catch(...)
		{}
	};

	migrate("ALTER TABLE team ADD COLUMN org_id INTEGER NOT NULL DEFAULT 0");
	migrate("ALTER TABLE team_member ADD COLUMN is_lead INTEGER NOT NULL DEFAULT 0");
	migrate("ALTER TABLE kanban_task ADD COLUMN type_id INTEGER NOT NULL DEFAULT 0");
	migrate("ALTER TABLE kanban_task ADD COLUMN is_archived INTEGER NOT NULL DEFAULT 0");
	migrate("ALTER TABLE team ADD COLUMN is_archived INTEGER NOT NULL DEFAULT 0");

	migrate(
	  "CREATE TABLE IF NOT EXISTS task_comment ("
	  "id integer primary key autoincrement,"
	  " task_id integer not null default 0,"
	  " author text not null default '',"
	  " body text not null default '',"
	  " created_at text not null default '',"
	  " is_deleted integer not null default 0)");

	migrate(
	  "CREATE TABLE IF NOT EXISTS task_comment_event ("
	  "id integer primary key autoincrement,"
	  " comment_id integer not null default 0,"
	  " actor text not null default '',"
	  " occurred_at text not null default '',"
	  " event_type text not null default '',"
	  " body_snapshot text not null default '')");
}

// ---- static helpers ----

team_entry kanban_db::to_entry(const Wt::Dbo::ptr<team_record>& p)
{
	return {.id = p.id(), .name = p->name, .org_id = p->org_id, .is_archived = p->is_archived != 0};
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
	e.is_archived = p->is_archived != 0;
	return e;
}

task_type_entry kanban_db::to_entry(const Wt::Dbo::ptr<task_type_record>& p)
{
	return {.id = p.id(), .org_id = p->org_id, .name = p->name, .color = p->color};
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
	  m_dbo.find<team_record>().where("is_archived = 0").orderBy("id").resultList();
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
	  m_dbo.find<team_record>().where("org_id = ? AND is_archived = 0").bind(org_id).orderBy("id").resultList();
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

void kanban_db::archive_team(long long id, const std::string& actor)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           teams =
	  m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
	if(teams.empty())
	{
		return;
	}
	(*teams.begin()).modify()->is_archived = 1;

	const auto tasks =
	  m_dbo.find<kanban_task_record>()
	    .where("team_id = ? AND is_archived = 0")
	    .bind(id)
	    .resultList();
	for(const auto& p: tasks)
	{
		p.modify()->is_archived = 1;
		record_event(p.id(), actor, "archived", {});
	}
}

void kanban_db::unarchive_team(long long id)
{
	// Tasks within the team are NOT automatically unarchived — each task must be
	// unarchived individually. This is intentional: tasks may have been archived
	// independently before the team was archived.
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<team_record>().where("id = ?").bind(id).resultList();
	if(!results.empty())
	{
		(*results.begin()).modify()->is_archived = 0;
	}
}

std::vector<team_entry> kanban_db::archived_teams_for_org(long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<team_record>()
	                       .where("org_id = ? AND is_archived = 1")
	                       .bind(org_id)
	                       .orderBy("id")
	                       .resultList();
	std::vector<team_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
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

long long kanban_db::add_task(const kanban_task_entry& e, const std::string& actor)
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
	const long long new_id = p.id();
	record_event(new_id, actor, "created", {});
	return new_id;
}

void kanban_db::update_task(const kanban_task_entry& e, const std::string& actor)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(e.id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();

	auto date_str = [](const Wt::WDate& d) -> std::string {
		return d.isValid() ? d.toString("yyyy-MM-dd").toUTF8() : "";
	};
	auto type_name = [&](long long tid) -> std::string {
		if(tid == 0)
		{
			return {};
		}
		try
		{
			return m_dbo.load<task_type_record>(tid)->name;
		}
		catch(const Wt::Dbo::ObjectNotFoundException&)
		{
			return {};
		}
	};

	std::vector<task_field_change_entry> changes;
	auto                                 diff = [&](const std::string& field,
                  const std::string& old_val,
                  const std::string& new_val) {
    if(old_val != new_val)
    {
      changes.push_back({field, old_val, new_val});
    }
	};

	diff("status", p->status, e.status);
	diff("title", p->title, e.title);
	diff("description", p->description, e.description);
	diff("assigned_to", p->assigned_to, e.assigned_to);
	diff("start_date", date_str(p->start_date), date_str(e.start_date));
	diff("end_date", date_str(p->end_date), date_str(e.end_date));
	diff("type", type_name(p->type_id), type_name(e.type_id));

	p.modify()->status      = e.status;
	p.modify()->title       = e.title;
	p.modify()->description = e.description;
	p.modify()->assigned_to = e.assigned_to;
	p.modify()->start_date  = e.start_date;
	p.modify()->end_date    = e.end_date;
	p.modify()->type_id     = e.type_id;
	p.modify()->sort_order  = e.sort_order;

	if(!changes.empty())
	{
		record_event(e.id, actor, "updated", changes);
	}
}

void kanban_db::update_task_status(long long          id,
                                   const std::string& status,
                                   int                sort_order,
                                   const std::string& actor)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<kanban_task_record> p          = *results.begin();
	const std::string                old_status = p->status;
	p.modify()->status                          = status;
	p.modify()->sort_order                      = sort_order;

	if(old_status != status)
	{
		record_event(id, actor, "updated", {{"status", old_status, status}});
	}
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
	if(p->is_archived)
	{
		return false;
	}
	p.modify()->assigned_to = username;
	record_event(task_id, username, "updated", {{"assigned_to", {}, username}});
	return true;
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
	                       .where("team_id = ? AND is_archived = 0")
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

std::vector<kanban_task_entry> kanban_db::archived_tasks_for_team(long long team_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<kanban_task_record>()
	                       .where("team_id = ? AND is_archived = 1")
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

void kanban_db::archive_task(long long id, const std::string& actor)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
	if(p->is_archived)
	{
		return; // Already archived — no-op.
	}
	p.modify()->is_archived = 1;
	record_event(id, actor, "archived", {});
}

void kanban_db::unarchive_task(long long id, const std::string& actor)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<kanban_task_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<kanban_task_record> p = *results.begin();
	if(!p->is_archived)
	{
		return; // Already active — no-op.
	}
	p.modify()->is_archived = 0;
	record_event(id, actor, "unarchived", {});
}

std::vector<task_event_entry> kanban_db::history_for_task(long long task_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           events = m_dbo.find<task_event_record>()
	                      .where("task_id = ?")
	                      .bind(task_id)
	                      .orderBy("id DESC")
	                      .resultList();
	std::vector<task_event_entry> out;
	for(const auto& ev: events)
	{
		task_event_entry entry;
		entry.id          = ev.id();
		entry.task_id     = ev->task_id;
		entry.actor       = ev->actor;
		entry.occurred_at = ev->occurred_at;
		entry.event_type  = ev->event_type;

		const auto changes = m_dbo.find<task_field_change_record>()
		                       .where("event_id = ?")
		                       .bind(ev.id())
		                       .resultList();
		for(const auto& ch: changes)
		{
			entry.changes.push_back(
			  {.field_name = ch->field_name, .old_value = ch->old_value, .new_value = ch->new_value});
		}
		out.push_back(std::move(entry));
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

// ---- Task types ----

long long kanban_db::create_task_type(long long          org_id,
                                      const std::string& name,
                                      const std::string& color)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p = m_dbo.add(std::make_unique<task_type_record>());
	p.modify()->org_id     = org_id;
	p.modify()->name       = name;
	p.modify()->color      = color;
	m_dbo.flush();
	return p.id();
}

void kanban_db::update_task_type(long long          id,
                                 const std::string& name,
                                 const std::string& color)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p = m_dbo.load<task_type_record>(id);
	p.modify()->name       = name;
	p.modify()->color      = color;
}

void kanban_db::delete_task_type(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	m_dbo.execute("UPDATE kanban_task SET type_id=0 WHERE type_id=?").bind(id);
	auto p = m_dbo.load<task_type_record>(id);
	p.remove();
}

std::vector<task_type_entry> kanban_db::types_for_org(long long org_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<task_type_record>()
	                       .where("org_id = ?")
	                       .bind(org_id)
	                       .orderBy("id")
	                       .resultList();
	std::vector<task_type_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

std::optional<task_type_entry> kanban_db::find_task_type(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	try
	{
		auto p = m_dbo.load<task_type_record>(id);
		return to_entry(p);
	}
	catch(const Wt::Dbo::ObjectNotFoundException&)
	{
		return std::nullopt;
	}
}

// ---- Private helpers ----

void kanban_db::record_event(long long                                   task_id,
                             const std::string&                          actor,
                             const std::string&                          event_type,
                             const std::vector<task_field_change_entry>& changes)
{
	// Generate an ISO-8601 UTC timestamp (thread-safe via Wt).
	const std::string ts =
	  Wt::WDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toUTF8();

	auto ev                  = m_dbo.add(std::make_unique<task_event_record>());
	ev.modify()->task_id     = task_id;
	ev.modify()->actor       = actor;
	ev.modify()->occurred_at = ts;
	ev.modify()->event_type  = event_type;
	m_dbo.flush();

	for(const auto& ch: changes)
	{
		auto fc                 = m_dbo.add(std::make_unique<task_field_change_record>());
		fc.modify()->event_id   = ev.id();
		fc.modify()->field_name = ch.field_name;
		fc.modify()->old_value  = ch.old_value;
		fc.modify()->new_value  = ch.new_value;
	}
}

void kanban_db::record_comment_event(long long          comment_id,
                                     const std::string& actor,
                                     const std::string& event_type,
                                     const std::string& body_snapshot)
{
	const std::string ts =
	  Wt::WDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toUTF8();

	auto ev                    = m_dbo.add(std::make_unique<task_comment_event_record>());
	ev.modify()->comment_id    = comment_id;
	ev.modify()->actor         = actor;
	ev.modify()->occurred_at   = ts;
	ev.modify()->event_type    = event_type;
	ev.modify()->body_snapshot = body_snapshot;
}

// ---- Comments ----

long long kanban_db::add_comment(long long          task_id,
                                 const std::string& author,
                                 const std::string& body)
{
	Wt::Dbo::Transaction t{m_dbo};
	const std::string    ts =
	  Wt::WDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toUTF8();

	auto p                 = m_dbo.add(std::make_unique<task_comment_record>());
	p.modify()->task_id    = task_id;
	p.modify()->author     = author;
	p.modify()->body       = body;
	p.modify()->created_at = ts;
	p.modify()->is_deleted = 0;
	m_dbo.flush();
	const long long new_id = p.id();

	record_comment_event(new_id, author, "created", body);
	return new_id;
}

void kanban_db::edit_comment(long long          comment_id,
                             const std::string& editor,
                             const std::string& new_body)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<task_comment_record>()
	                       .where("id = ?")
	                       .bind(comment_id)
	                       .resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<task_comment_record> p = *results.begin();
	if(p->is_deleted)
	{
		return; // No-op on deleted comment.
	}
	p.modify()->body = new_body;
	record_comment_event(comment_id, editor, "edited", new_body);
}

void kanban_db::delete_comment(long long comment_id, const std::string& actor)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<task_comment_record>()
	                       .where("id = ?")
	                       .bind(comment_id)
	                       .resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<task_comment_record> p = *results.begin();
	if(p->is_deleted)
	{
		return; // Already deleted — no-op.
	}
	const std::string body_snapshot = p->body;
	p.modify()->is_deleted          = 1;
	record_comment_event(comment_id, actor, "deleted", body_snapshot);
}

std::vector<task_comment_entry> kanban_db::comments_for_task(long long task_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           rows = m_dbo.find<task_comment_record>()
	                    .where("task_id = ?")
	                    .bind(task_id)
	                    .orderBy("id")
	                    .resultList();

	std::vector<task_comment_entry> out;
	for(const auto& c: rows)
	{
		task_comment_entry e;
		e.id         = c.id();
		e.task_id    = c->task_id;
		e.author     = c->author;
		e.created_at = c->created_at;
		e.is_deleted = c->is_deleted != 0;
		e.body       = e.is_deleted ? "" : c->body;

		const auto events = m_dbo.find<task_comment_event_record>()
		                      .where("comment_id = ?")
		                      .bind(c.id())
		                      .orderBy("id")
		                      .resultList();
		for(const auto& ev: events)
		{
			if(ev->event_type == "edited")
			{
				e.last_edited_by = ev->actor;
				e.last_edited_at = ev->occurred_at;
			}
			else if(ev->event_type == "deleted")
			{
				e.deleted_by = ev->actor;
				e.deleted_at = ev->occurred_at;
			}
		}
		out.push_back(std::move(e));
	}
	return out;
}
