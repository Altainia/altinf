#include "org_landing_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <algorithm>
#include <ranges>
#include <string>

#include "paths.hpp"
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
	  [this, alive = m_alive] {
		  if(*alive)
		  {
			  refresh();
			  Wt::WApplication::instance()->triggerUpdate();
		  }
	  });
}

org_landing_page::~org_landing_page()
{
	*m_alive = false;
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
		addNew<Wt::WText>("Organization not found.", Wt::TextFormat::Plain);
		return;
	}

	addNew<Wt::WText>("<h1>" + org->name + "</h1>", Wt::TextFormat::UnsafeXHTML);

	if(m_is_org_lead)
	{
		auto* actions = addNew<Wt::WContainerWidget>();
		actions->setStyleClass("org-lead-actions");

		actions->addNew<Wt::WAnchor>(
		         Wt::WLink{Wt::LinkType::InternalPath, paths::org_edit(m_org_id)},
		         "Manage organization")
		  ->setStyleClass("editor-btn");

		actions->addNew<Wt::WAnchor>(
		         Wt::WLink{Wt::LinkType::InternalPath, paths::org_board(m_org_id)},
		         "View all teams\xe2\x80\x99 board")
		  ->setStyleClass("editor-btn editor-btn-cancel");

		actions->addNew<Wt::WAnchor>(
		         Wt::WLink{Wt::LinkType::InternalPath, paths::org_types(m_org_id)},
		         "Manage types")
		  ->setStyleClass("editor-btn editor-btn-cancel");
	}

	const auto  all_teams = m_kdb.teams_for_org(m_org_id);
	const auto& username  = m_session.username;

	addNew<Wt::WText>("<h2>Your teams</h2>", Wt::TextFormat::UnsafeXHTML);

	bool       has_own        = false;
	const auto is_team_member = [this, &username](const auto& t) { return m_kdb.is_member(t.id, username) || m_is_org_lead; };
	for(const auto& t: all_teams | std::views::filter(is_team_member))
	{
		has_own   = true;
		auto* row = addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-team-row");
		row->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, paths::team_kanban(t.id)},
		     t.name)
		  ->setStyleClass("org-team-link");
	}
	if(!has_own)
	{
		addNew<Wt::WText>("You are not a member of any team in this organization.",
		                  Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
	}

	bool has_other = false;
	for(const auto& t: all_teams | std::views::filter(std::not_fn(is_team_member)))
	{
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

	render_assigned_tasks(all_teams, username);
}

void org_landing_page::render_assigned_tasks(const std::vector<team_entry>& all_teams,
                                             const std::string&             username)
{
	addNew<Wt::WText>("<h2>Your tasks</h2>", Wt::TextFormat::UnsafeXHTML);

	// Built from a fresh query inside render(); refresh() re-renders on live_hub
	// updates, so assignment/date/archival changes appear without extra wiring.
	const auto my_tasks = m_kdb.assigned_tasks_for_user_in_org(username, m_org_id);
	if(my_tasks.empty())
	{
		addNew<Wt::WText>("You have no assigned tasks.", Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
		return;
	}

	// Group by team, in the order teams are listed; show only teams with tasks.
	for(const auto& team: all_teams)
	{
		const auto in_team = [&team](const auto& t) { return t.team_id == team.id; };
		if(std::ranges::none_of(my_tasks, in_team))
		{
			continue;
		}

		addNew<Wt::WText>(team.name, Wt::TextFormat::Plain)
		  ->setStyleClass("org-tasks-team");

		for(const auto& task: my_tasks | std::views::filter(in_team))
		{
			auto* row = addNew<Wt::WAnchor>(
			  Wt::WLink{Wt::LinkType::InternalPath, paths::task_edit(task.id)});
			row->setStyleClass("org-task-row");
			row->addNew<Wt::WText>(task.title, Wt::TextFormat::Plain)
			  ->setStyleClass("org-task-title");

			const std::string due =
			  task.end_date.isValid() ? task.end_date.toString("yyyy-MM-dd").toUTF8() : "No end date";
			row->addNew<Wt::WText>(due, Wt::TextFormat::Plain)
			  ->setStyleClass("org-task-due");
		}
	}
}

void org_landing_page::refresh()
{
	clear();
	render();
}