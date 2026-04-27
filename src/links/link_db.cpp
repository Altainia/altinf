#include "link_db.hpp"

#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>

link_db::link_db(const std::string& db_path)
{
	m_dbo.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
	m_dbo.mapClass<link_record>("link");

	try
	{
		m_dbo.createTables();
	}
	catch(const Wt::Dbo::Exception&)
	{
		// Tables already exist — ignore
	}
}

std::vector<link_entry> link_db::load_all()
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<link_record>().orderBy("section, sort_order, added_date").resultList();

	std::vector<link_entry> out;

	for(const Wt::Dbo::ptr<link_record>& p: results)
	{
		link_entry e;
		e.id          = p.id();
		e.url         = p->url;
		e.title       = p->title;
		e.description = p->description;
		e.section     = p->section;
		e.sort_order  = p->sort_order;
		e.added_date  = p->added_date;
		out.push_back(std::move(e));
	}

	return out;
}

std::optional<link_entry> link_db::find(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<link_record>().where("id = ?").bind(id).resultList();

	if(results.empty())
	{
		return std::nullopt;
	}

	const Wt::Dbo::ptr<link_record>& p = *results.begin();
	link_entry                       e;
	e.id          = p.id();
	e.url         = p->url;
	e.title       = p->title;
	e.description = p->description;
	e.section     = p->section;
	e.sort_order  = p->sort_order;
	e.added_date  = p->added_date;
	return e;
}

long long link_db::add(const link_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};

	auto p                  = m_dbo.add(std::make_unique<link_record>());
	p.modify()->url         = e.url;
	p.modify()->title       = e.title;
	p.modify()->description = e.description;
	p.modify()->section     = e.section;
	p.modify()->sort_order  = e.sort_order;
	p.modify()->added_date  = e.added_date;
	m_dbo.flush(); // force INSERT so the DB-assigned rowid is readable via id()

	return p.id();
}

void link_db::update(const link_entry& e)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<link_record>().where("id = ?").bind(e.id).resultList();

	if(results.empty())
	{
		return;
	}

	Wt::Dbo::ptr<link_record> p = *results.begin();
	p.modify()->url             = e.url;
	p.modify()->title           = e.title;
	p.modify()->description     = e.description;
	p.modify()->section         = e.section;
	p.modify()->sort_order      = e.sort_order;
	p.modify()->added_date      = e.added_date;
}

void link_db::remove(long long id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<link_record>().where("id = ?").bind(id).resultList();

	if(!results.empty())
	{
		(*results.begin()).remove();
	}
}
