#include "kanban_board_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "kanban/gantt_view_widget.hpp"
#include "widgets/live_hub.hpp"

kanban_board_page::kanban_board_page(kanban_db&          db,
                                     const session_data& session,
                                     long long           team_id,
                                     bool                is_lead,
                                     bool                show_gantt):
  m_db{db},
  m_team_id{team_id},
  m_is_lead{is_lead},
  m_show_gantt{show_gantt}
{
	setStyleClass("page kb-page");

	const auto team = db.find_team(team_id);
	if(!team)
	{
		addNew<Wt::WText>("Team not found.", Wt::TextFormat::Plain);
		return;
	}

	const std::string team_url = "/board/" + std::to_string(team_id);

	// ── Header ────────────────────────────────────────────────────────────────
	auto* hdr = addNew<Wt::WContainerWidget>();
	hdr->setStyleClass("kb-page-hdr");

	hdr->addNew<Wt::WText>("<h1>" + team->name + "</h1>",
	                       Wt::TextFormat::UnsafeXHTML);

	auto* tabs = hdr->addNew<Wt::WContainerWidget>();
	tabs->setStyleClass("kb-view-tabs");

	tabs->addNew<Wt::WAnchor>(
	      Wt::WLink{Wt::LinkType::InternalPath, team_url}, "Board")
	  ->setStyleClass(show_gantt ? "kb-tab" : "kb-tab kb-tab--active");

	tabs->addNew<Wt::WAnchor>(
	      Wt::WLink{Wt::LinkType::InternalPath, team_url + "/gantt"}, "Gantt")
	  ->setStyleClass(show_gantt ? "kb-tab kb-tab--active" : "kb-tab");

	if(m_is_lead)
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, team_url + "/task/new"},
		     "+ New Task")
		  ->setStyleClass("editor-btn kb-new-btn");

		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, team_url + "/manage"},
		     "Manage Team")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
	}

	// ── Content ───────────────────────────────────────────────────────────────
	const auto tasks = db.tasks_for_team(team_id);

	if(show_gantt)
	{
		m_gantt_widget = addNew<gantt_view_widget>(tasks);
	}
	else
	{
		m_board_widget = addNew<kanban_board_widget>(
		  tasks,
		  m_is_lead,
		  [this](long long tid, const std::string& status, int sort) {
			  m_db.update_task_status(tid, status, sort);
			  live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
			  m_board_widget->refresh(m_db.tasks_for_team(m_team_id), m_is_lead);
		  },
		  [this](long long tid) {
			  Wt::WApplication::instance()->setInternalPath(
			    "/board/" + std::to_string(m_team_id) + "/task/" +
			      std::to_string(tid) + "/edit",
			    true);
		  });
	}

	// Subscribe to live updates for this team.
	m_session_id = Wt::WApplication::instance()->sessionId();
	live_hub::instance().subscribe(
	  "team:" + std::to_string(m_team_id),
	  m_session_id,
	  [this] { refresh(); Wt::WApplication::instance()->triggerUpdate(); });
}

kanban_board_page::~kanban_board_page()
{
	if(!m_session_id.empty())
	{
		live_hub::instance().unsubscribe(
		  "team:" + std::to_string(m_team_id), m_session_id);
	}
}

void kanban_board_page::refresh()
{
	const auto tasks = m_db.tasks_for_team(m_team_id);
	if(m_show_gantt && m_gantt_widget)
	{
		m_gantt_widget->refresh(tasks);
	}
	else if(m_board_widget)
	{
		m_board_widget->refresh(tasks, m_is_lead);
	}
}
