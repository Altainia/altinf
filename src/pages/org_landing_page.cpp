#include "org_landing_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "widgets/live_hub.hpp"

org_landing_page::org_landing_page(org_db&             odb,
                                   kanban_db&          kdb,
                                   long long           org_id,
                                   const session_data& session,
                                   bool                is_org_lead):
  m_odb{odb},
  m_kdb{kdb},
  m_org_id{org_id},
  m_session{session},
  m_is_org_lead{is_org_lead}
{
	setStyleClass("page org-landing-page");
	render();

	m_session_id = Wt::WApplication::instance()->sessionId();
	live_hub::instance().subscribe(
	  "org:" + std::to_string(m_org_id),
	  m_session_id,
	  [this] { refresh(); Wt::WApplication::instance()->triggerUpdate(); });
}

org_landing_page::~org_landing_page()
{
	if(!m_session_id.empty())
	{
		live_hub::instance().unsubscribe(
		  "org:" + std::to_string(m_org_id), m_session_id);
	}
}

void org_landing_page::render()
{
	const auto org = m_odb.find_org(m_org_id);
	if(!org)
	{
		addNew<Wt::WText>("Organisation not found.", Wt::TextFormat::Plain);
		return;
	}

	addNew<Wt::WText>("<h1>" + org->name + "</h1>", Wt::TextFormat::UnsafeXHTML);

	if(m_is_org_lead)
	{
		auto* actions = addNew<Wt::WContainerWidget>();
		actions->setStyleClass("org-lead-actions");

		actions->addNew<Wt::WAnchor>(
		         Wt::WLink{Wt::LinkType::InternalPath,
		                   "/org/" + std::to_string(m_org_id) + "/manage"},
		         "Manage organisation")
		  ->setStyleClass("editor-btn");

		actions->addNew<Wt::WAnchor>(
		         Wt::WLink{Wt::LinkType::InternalPath,
		                   "/org/" + std::to_string(m_org_id) + "/board"},
		         "View all teams\xe2\x80\x99 board")
		  ->setStyleClass("editor-btn editor-btn-cancel");
	}

	const auto  all_teams = m_kdb.teams_for_org(m_org_id);
	const auto& username  = m_session.username;

	addNew<Wt::WText>("<h2>Your teams</h2>", Wt::TextFormat::UnsafeXHTML);

	bool has_own = false;
	for(const auto& t: all_teams)
	{
		if(!m_kdb.is_member(t.id, username) && !m_is_org_lead)
		{
			continue;
		}
		has_own   = true;
		auto* row = addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-team-row");
		row->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath,
		               "/board/" + std::to_string(t.id)},
		     t.name)
		  ->setStyleClass("org-team-link");
	}
	if(!has_own)
	{
		addNew<Wt::WText>("You are not a member of any team in this organisation.",
		                  Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
	}

	bool has_other = false;
	for(const auto& t: all_teams)
	{
		if(m_kdb.is_member(t.id, username) || m_is_org_lead)
		{
			continue;
		}
		if(!has_other)
		{
			addNew<Wt::WText>("<h2>Other teams</h2>", Wt::TextFormat::UnsafeXHTML);
			has_other = true;
		}
		auto* row = addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-team-row org-team-row--other");
		row->addNew<Wt::WText>(t.name, Wt::TextFormat::Plain)
		  ->setStyleClass("org-team-name");
	}
}

void org_landing_page::refresh()
{
	clear();
	render();
}
