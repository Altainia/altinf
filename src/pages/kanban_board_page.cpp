#include "kanban_board_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "auth/permission.hpp"
#include "kanban/gantt_view_widget.hpp"

kanban_board_page::kanban_board_page(kanban_db& db, const session_data& session, bool show_gantt):
  m_db{db}
{
	setStyleClass("page kb-page");

	const auto teams = db.all_teams();
	if(teams.empty())
	{
		addNew<Wt::WText>("No team configured.", Wt::TextFormat::Plain);
		return;
	}

	m_team_id  = teams[0].id;
	m_can_edit = db.can_edit_board(m_team_id, session.username, session.permissions);

	if(!db.can_view_board(m_team_id, session.username, session.permissions))
	{
		addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
		return;
	}

	// ── Header ────────────────────────────────────────────────────────────────
	auto* hdr = addNew<Wt::WContainerWidget>();
	hdr->setStyleClass("kb-page-hdr");

	hdr->addNew<Wt::WText>("<h1>" + teams[0].name + "</h1>", Wt::TextFormat::UnsafeXHTML);

	auto* tabs = hdr->addNew<Wt::WContainerWidget>();
	tabs->setStyleClass("kb-view-tabs");

	auto* board_tab = tabs->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/board"}, "Board");
	board_tab->setStyleClass(show_gantt ? "kb-tab" : "kb-tab kb-tab--active");

	auto* gantt_tab = tabs->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/board/gantt"}, "Gantt");
	gantt_tab->setStyleClass(show_gantt ? "kb-tab kb-tab--active" : "kb-tab");

	if(m_can_edit)
	{
		auto* new_btn = hdr->addNew<Wt::WAnchor>(
		  Wt::WLink{Wt::LinkType::InternalPath, "/board/task/new"}, "+ New Task");
		new_btn->setStyleClass("editor-btn kb-new-btn");
	}

	if(session.permissions.has_any(permission::admin))
	{
		auto* team_link = hdr->addNew<Wt::WAnchor>(
		  Wt::WLink{Wt::LinkType::InternalPath, "/admin/board/team"}, "Manage Team");
		team_link->setStyleClass("kb-manage-link");
	}

	// ── Content ───────────────────────────────────────────────────────────────
	const auto tasks = db.tasks_for_team(m_team_id);

	if(show_gantt)
	{
		addNew<gantt_view_widget>(tasks);
	}
	else
	{
		m_board_widget = addNew<kanban_board_widget>(
		  tasks,
		  m_can_edit,
		  [this](long long tid, const std::string& status, int sort) {
			  m_db.update_task_status(tid, status, sort);
			  m_board_widget->refresh(m_db.tasks_for_team(m_team_id), m_can_edit);
		  },
		  [](long long tid) {
			  Wt::WApplication::instance()->setInternalPath(
			    "/board/task/" + std::to_string(tid) + "/edit", true);
		  });
	}
}
