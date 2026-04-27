#include "gantt_db.hpp"

#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>

#include <vector>

#include "auth/permission.hpp"

gantt_db::gantt_db(const std::string& db_path)
{
	m_dbo.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
	m_dbo.mapClass<gantt_project_record>("gantt_project");
	m_dbo.mapClass<gantt_task_record>("gantt_task");
	m_dbo.mapClass<gantt_viewer_record>("gantt_viewer");

	try
	{
		m_dbo.createTables();
	}
	catch(const Wt::Dbo::Exception&)
	{
		// Tables already exist — ignore
	}
}

// ---- static helpers ----

gantt_project_entry gantt_db::to_entry(const Wt::Dbo::ptr<gantt_project_record>& p)
{
	gantt_project_entry e;
	e.id             = p.id();
	e.title          = p->title;
	e.description    = p->description;
	e.owner_username = p->owner_username;
	e.created_date   = p->created_date;
	return e;
}

gantt_task_entry gantt_db::to_entry(const Wt::Dbo::ptr<gantt_task_record>& p)
{
	gantt_task_entry e;
	e.id          = p.id();
	e.project_id  = p->project_id;
	e.title       = p->title;
	e.assigned_to = p->assigned_to;
	e.start_date  = p->start_date;
	e.end_date    = p->end_date;
	e.color       = p->color;
	e.sort_order  = p->sort_order;
	return e;
}

// ---- Projects ----

long long gantt_db::create_project(const gantt_project_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p     = m_dbo.add(std::make_unique<gantt_project_record>());
	p.modify()->title          = e.title;
	p.modify()->description    = e.description;
	p.modify()->owner_username = e.owner_username;
	p.modify()->created_date   = e.created_date;
	m_dbo.flush(); // force INSERT so the DB-assigned rowid is readable via id()
	return p.id();
}

void gantt_db::update_project(const gantt_project_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<gantt_project_record>().where("id = ?").bind(e.id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<gantt_project_record> p = *results.begin();
	p.modify()->title                    = e.title;
	p.modify()->description              = e.description;
	p.modify()->owner_username           = e.owner_username;
	p.modify()->created_date             = e.created_date;
}

void gantt_db::delete_project(long long id)
{
	delete_tasks_for_project(id);
	delete_viewers_for_project(id);
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<gantt_project_record>().where("id = ?").bind(id).resultList();
	if(!results.empty())
	{
		Wt::Dbo::ptr<gantt_project_record> p = *results.begin();
		p.remove();
	}
}

std::optional<gantt_project_entry> gantt_db::find_project(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<gantt_project_record>().where("id = ?").bind(id).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	return to_entry(*results.begin());
}

std::vector<gantt_project_entry> gantt_db::projects_visible_to(const std::string& username,
                                                               permission::flags  perms)
{
	Wt::Dbo::Transaction t{m_dbo};

	Wt::Dbo::collection<Wt::Dbo::ptr<gantt_project_record>> results;
	if(perms.has_any(permission::admin))
	{
		results = m_dbo.find<gantt_project_record>().orderBy("id").resultList();
	}
	else
	{
		results = m_dbo.find<gantt_project_record>()
		            .where(
		              "owner_username = ? OR id IN "
		              "(SELECT project_id FROM gantt_viewer WHERE username = ?)")
		            .bind(username)
		            .bind(username)
		            .orderBy("id")
		            .resultList();
	}

	std::vector<gantt_project_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

// ---- Tasks ----

long long gantt_db::add_task(const gantt_task_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p  = m_dbo.add(std::make_unique<gantt_task_record>());
	p.modify()->project_id  = e.project_id;
	p.modify()->title       = e.title;
	p.modify()->assigned_to = e.assigned_to;
	p.modify()->start_date  = e.start_date;
	p.modify()->end_date    = e.end_date;
	p.modify()->color       = e.color;
	p.modify()->sort_order  = e.sort_order;
	m_dbo.flush(); // force INSERT so the DB-assigned rowid is readable via id()
	return p.id();
}

void gantt_db::update_task(const gantt_task_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<gantt_task_record>().where("id = ?").bind(e.id).resultList();
	if(results.empty())
	{
		return;
	}
	Wt::Dbo::ptr<gantt_task_record> p = *results.begin();
	p.modify()->title                 = e.title;
	p.modify()->assigned_to           = e.assigned_to;
	p.modify()->start_date            = e.start_date;
	p.modify()->end_date              = e.end_date;
	p.modify()->color                 = e.color;
	p.modify()->sort_order            = e.sort_order;
}

void gantt_db::delete_task(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<gantt_task_record>().where("id = ?").bind(id).resultList();
	if(!results.empty())
	{
		Wt::Dbo::ptr<gantt_task_record> p = *results.begin();
		p.remove();
	}
}

void gantt_db::delete_tasks_for_project(long long project_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<gantt_task_record>()
	                       .where("project_id = ?")
	                       .bind(project_id)
	                       .resultList();
	std::vector<Wt::Dbo::ptr<gantt_task_record>> ptrs(results.begin(), results.end());
	for(auto& p: ptrs)
	{
		p.remove();
	}
}

std::vector<gantt_task_entry> gantt_db::tasks_for_project(long long project_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<gantt_task_record>()
	                       .where("project_id = ?")
	                       .bind(project_id)
	                       .orderBy("sort_order, id")
	                       .resultList();
	std::vector<gantt_task_entry> out;
	for(const auto& p: results)
	{
		out.push_back(to_entry(p));
	}
	return out;
}

// ---- Viewers ----

void gantt_db::add_viewer(long long project_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	auto                 p = m_dbo.add(std::make_unique<gantt_viewer_record>());
	p.modify()->project_id = project_id;
	p.modify()->username   = username;
}

void gantt_db::remove_viewer(long long project_id, const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<gantt_viewer_record>()
	                       .where("project_id = ? AND username = ?")
	                       .bind(project_id)
	                       .bind(username)
	                       .resultList();
	if(!results.empty())
	{
		Wt::Dbo::ptr<gantt_viewer_record> p = *results.begin();
		p.remove();
	}
}

void gantt_db::delete_viewers_for_project(long long project_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<gantt_viewer_record>()
	                       .where("project_id = ?")
	                       .bind(project_id)
	                       .resultList();
	std::vector<Wt::Dbo::ptr<gantt_viewer_record>> ptrs(results.begin(), results.end());
	for(auto& p: ptrs)
	{
		p.remove();
	}
}

std::vector<std::string> gantt_db::viewers_for_project(long long project_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<gantt_viewer_record>()
	                       .where("project_id = ?")
	                       .bind(project_id)
	                       .resultList();
	std::vector<std::string> out;
	for(const auto& p: results)
	{
		out.push_back(p->username);
	}
	return out;
}

// ---- Permission helpers ----

bool gantt_db::can_view(long long project_id, const std::string& username, permission::flags perms)
{
	if(perms.has_any(permission::admin))
	{
		return true;
	}

	const auto opt = find_project(project_id);
	if(!opt)
	{
		return false;
	}
	if(opt->owner_username == username)
	{
		return true;
	}

	Wt::Dbo::Transaction t{m_dbo};
	const auto           results = m_dbo.find<gantt_viewer_record>()
	                       .where("project_id = ? AND username = ?")
	                       .bind(project_id)
	                       .bind(username)
	                       .resultList();
	return !results.empty();
}

bool gantt_db::can_edit(long long project_id, const std::string& username, permission::flags perms)
{
	if(perms.has_any(permission::admin))
	{
		return true;
	}
	if(!perms.has_any(permission::gantt_write))
	{
		return false;
	}

	const auto opt = find_project(project_id);
	return opt && opt->owner_username == username;
}
