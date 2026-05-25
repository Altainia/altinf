#include "team_kanban_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <map>

#include "org/widgets/gantt_view_widget.hpp"
#include "org/widgets/task_popup_widget.hpp"
#include "paths.hpp"
#include "widgets/live_hub.hpp"

team_kanban_page::team_kanban_page(kanban_db&          db,
                                   org_db&             odb,
                                   const session_data& session,
                                   long long           team_id,
                                   team_cap::flags     caps,
                                   team_settings_entry settings,
                                   bool                show_gantt):
  m_db{db},
  m_odb{odb},
  m_session{session},
  m_team_id{team_id},
  m_caps{caps},
  m_settings{settings},
  m_show_gantt{show_gantt},
  m_username{session.username}
{
	setStyleClass("page kb-page");

	const auto team = db.find_team(team_id);
	if(!team)
	{
		addNew<Wt::WText>("Team not found.", Wt::TextFormat::Plain);
		return;
	}

	m_org_id = team->org_id;
	for(const auto& ty: db.types_for_org(m_org_id))
	{
		m_type_colors[ty.id] = ty.color;
	}

	// ── Header ────────────────────────────────────────────────────────────────
	auto* hdr = addNew<Wt::WContainerWidget>();
	hdr->setStyleClass("kb-page-hdr");

	hdr->addNew<Wt::WText>("<h1>" + team->name + "</h1>",
	                       Wt::TextFormat::UnsafeXHTML);

	auto* tabs = hdr->addNew<Wt::WContainerWidget>();
	tabs->setStyleClass("kb-view-tabs");

	tabs->addNew<Wt::WAnchor>(
	      Wt::WLink{Wt::LinkType::InternalPath, paths::team_kanban(team_id)}, "Board")
	  ->setStyleClass(show_gantt ? "kb-tab" : "kb-tab kb-tab--active");

	tabs->addNew<Wt::WAnchor>(
	      Wt::WLink{Wt::LinkType::InternalPath, paths::team_gantt(team_id)}, "Gantt")
	  ->setStyleClass(show_gantt ? "kb-tab kb-tab--active" : "kb-tab");

	if(caps.has_any(team_cap::create_task))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, paths::team_task_new(team_id)},
		     "+ New Task")
		  ->setStyleClass("editor-btn kb-new-btn");
	}

	if(caps.has_any(team_cap::manage_team))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, paths::team_edit_members(team_id)},
		     "Manage Team")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");

		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, paths::team_edit_settings(team_id)},
		     "Settings")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
	}

	if(caps.has_any(team_cap::view_archived))
	{
		hdr->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, paths::team_archive(team_id)},
		     "Archived")
		  ->setStyleClass("editor-btn editor-btn-cancel kb-manage-link");
	}

	// ── Content ───────────────────────────────────────────────────────────────
	const auto tasks = db.tasks_for_team(team_id);

	const bool is_lead          = m_caps.has_any(team_cap::edit_task_details);
	const bool can_move_columns = is_lead ||
	                              (m_caps.has_any(team_cap::edit_task_fields) &&
	                               m_settings.allow_member_move_columns);
	const bool can_move_done = is_lead;

	if(show_gantt)
	{
		m_gantt_widget = addNew<gantt_view_widget>(
		  tasks,
		  m_type_colors,
		  [this](long long tid) {
			  new task_popup_widget(
			    m_db, m_odb, tid, m_session, m_caps, m_settings, m_team_id);
		  });
	}
	else
	{
		m_board_widget = addNew<kanban_board_widget>(
		  tasks,
		  can_move_columns,
		  can_move_done,
		  m_type_colors,
		  [this, can_move_columns, can_move_done](long long tid, const std::string& status, int sort) {
			  const auto task = m_db.find_task(tid);
			  if(!task)
			  {
				  return;
			  }
			  const bool from_done = (task->status == "done");
			  const bool to_done   = (status == "done");
			  if((from_done || to_done) && !can_move_done)
			  {
				  return;
			  }
			  if(!from_done && !to_done && !can_move_columns)
			  {
				  return;
			  }
			  m_db.update_task_status(tid, status, sort, m_username);
			  live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
		  },
		  [this](long long tid) {
			  new task_popup_widget(
			    m_db, m_odb, tid, m_session, m_caps, m_settings, m_team_id);
		  });
	}

	// Subscribe to live updates for this team.
	m_session_id = Wt::WApplication::instance()->sessionId();
	live_hub::instance().subscribe(
	  "team:" + std::to_string(m_team_id),
	  m_session_id,
	  [this, alive = m_alive] {
		  if(*alive)
		  {
			  refresh();
			  Wt::WApplication::instance()->triggerUpdate();
		  }
	  });
	live_hub::instance().subscribe(
	  "org:" + std::to_string(m_org_id) + ":types",
	  m_session_id,
	  [this, alive = m_alive] {
		  if(*alive)
		  {
			  refresh();
			  Wt::WApplication::instance()->triggerUpdate();
		  }
	  });
}

team_kanban_page::~team_kanban_page()
{
	*m_alive = false;
	if(!m_session_id.empty())
	{
		live_hub::instance().unsubscribe(
		  "team:" + std::to_string(m_team_id), m_session_id);
		if(m_org_id != 0)
		{
			live_hub::instance().unsubscribe(
			  "org:" + std::to_string(m_org_id) + ":types", m_session_id);
		}
	}
}

void team_kanban_page::refresh()
{
	m_type_colors.clear();
	for(const auto& ty: m_db.types_for_org(m_org_id))
	{
		m_type_colors[ty.id] = ty.color;
	}

	const bool is_lead          = m_caps.has_any(team_cap::edit_task_details);
	const bool can_move_columns = is_lead ||
	                              (m_caps.has_any(team_cap::edit_task_fields) &&
	                               m_settings.allow_member_move_columns);
	const bool can_move_done = is_lead;

	const auto tasks = m_db.tasks_for_team(m_team_id);
	if(m_show_gantt && m_gantt_widget)
	{
		m_gantt_widget->refresh(tasks, m_type_colors);
	}
	else if(m_board_widget)
	{
		m_board_widget->refresh(tasks, can_move_columns, can_move_done, m_type_colors);
	}
}