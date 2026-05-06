#include "kanban_task_editor_page.hpp"

#include <Wt/Dbo/Exception.h>
#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WColor.h>
#include <Wt/WDate.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>
#include <cstdio>

#include "org/org.hpp"
#include "widgets/live_hub.hpp"

const std::vector<std::string> kanban_task_editor_page::k_status_vals = {
  "todo",
  "in_progress",
  "review",
  "done"};
const std::vector<std::string> kanban_task_editor_page::k_status_labels = {
  "To Do",
  "In Progress",
  "Review",
  "Done"};

kanban_task_editor_page::kanban_task_editor_page(
  kanban_db&                          db,
  org_db&                             odb,
  long long                           team_id,
  const session_data&                 session,
  bool                                is_lead,
  const kanban_task_entry*            existing,
  const std::vector<std::string>&     members,
  const std::vector<task_type_entry>& types,
  std::function<void()>               on_save):
  m_db{db},
  m_odb{odb},
  m_team_id{team_id},
  m_username{session.username},
  m_is_lead{is_lead},
  m_existing{existing},
  m_on_save{std::move(on_save)}
{
	setStyleClass("page kb-editor-page");

	const bool        is_new           = (existing == nullptr);
	const std::string current_assignee = existing ? existing->assigned_to : "";
	const bool        locked_out =
	  !is_lead && !current_assignee.empty() &&
	  current_assignee != session.username;

	addNew<Wt::WText>(
	  is_new ? "<h1>New Task</h1>" : "<h1>Edit Task</h1>",
	  Wt::TextFormat::UnsafeXHTML);

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("kb-editor-form");

	// ── Title ─────────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Task</h2>", Wt::TextFormat::UnsafeXHTML);

	m_title = form->addNew<Wt::WLineEdit>();
	m_title->setPlaceholderText("Task title");
	m_title->setStyleClass("editor-field");
	if(existing)
	{
		m_title->setText(existing->title);
	}
	m_title->setReadOnly(!is_lead && existing != nullptr);

	m_description = form->addNew<Wt::WTextArea>();
	m_description->setPlaceholderText("Description (optional)");
	m_description->setStyleClass("editor-field kb-desc-field");
	if(existing)
	{
		m_description->setText(existing->description);
	}

	// ── Assignment & status ───────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Assignment</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* row = form->addNew<Wt::WContainerWidget>();
	row->setStyleClass("kb-editor-row");

	auto* status_wrap = row->addNew<Wt::WContainerWidget>();
	status_wrap->setStyleClass("kb-editor-field-wrap");
	status_wrap->addNew<Wt::WText>("Status", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	m_status = status_wrap->addNew<Wt::WComboBox>();
	m_status->setStyleClass("editor-field");
	for(const auto& lbl: k_status_labels)
	{
		m_status->addItem(lbl);
	}
	if(existing)
	{
		const auto it =
		  std::find(k_status_vals.begin(), k_status_vals.end(), existing->status);
		if(it != k_status_vals.end())
		{
			m_status->setCurrentIndex(
			  static_cast<int>(std::distance(k_status_vals.begin(), it)));
		}
	}

	auto* assignee_wrap = row->addNew<Wt::WContainerWidget>();
	assignee_wrap->setStyleClass("kb-editor-field-wrap");
	assignee_wrap->addNew<Wt::WText>("Assigned to", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	m_assigned_to = assignee_wrap->addNew<Wt::WComboBox>();
	m_assigned_to->setStyleClass("editor-field");

	// Build the assignee dropdown based on permission level.
	m_assignee_values.push_back(""); // index 0 = unassigned
	m_assigned_to->addItem("(unassigned)");

	if(is_lead)
	{
		// Full member list.
		for(const auto& m: members)
		{
			m_assignee_values.push_back(m);
			m_assigned_to->addItem(m);
		}
	}
	else
	{
		// Only the current user can appear as an option.
		m_assignee_values.push_back(session.username);
		m_assigned_to->addItem(session.username);
	}

	// Set current selection.
	if(existing && !existing->assigned_to.empty())
	{
		const auto it = std::find(
		  m_assignee_values.begin(), m_assignee_values.end(), existing->assigned_to);
		if(it != m_assignee_values.end())
		{
			m_assigned_to->setCurrentIndex(
			  static_cast<int>(std::distance(m_assignee_values.begin(), it)));
		}
	}

	// Disable the dropdown if the task is assigned to someone else and user
	// is not a lead — they cannot touch the assignment.
	if(locked_out)
	{
		m_assigned_to->setDisabled(true);
	}

	// ── Dates & color ─────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Schedule</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* sched_row = form->addNew<Wt::WContainerWidget>();
	sched_row->setStyleClass("kb-editor-row");

	auto* start_wrap = sched_row->addNew<Wt::WContainerWidget>();
	start_wrap->setStyleClass("kb-editor-field-wrap");
	start_wrap->addNew<Wt::WText>("Start date", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	auto* start_row = start_wrap->addNew<Wt::WContainerWidget>();
	start_row->setStyleClass("kb-date-row");
	m_start_date = start_row->addNew<Wt::WDateEdit>();
	m_start_date->setFormat("yyyy-MM-dd");
	m_start_date->setStyleClass("editor-field");
	m_start_date->changed().connect([] {});
	if(existing && existing->start_date.isValid())
	{
		m_start_date->setDate(existing->start_date);
	}
	else if(!existing)
	{
		m_start_date->setDate(Wt::WDate::currentDate());
	}
	auto* clear_start = start_row->addNew<Wt::WPushButton>("Clear");
	clear_start->setStyleClass("kb-date-clear");
	clear_start->clicked().connect([this] { m_start_date->setText(Wt::WString{}); });

	auto* end_wrap = sched_row->addNew<Wt::WContainerWidget>();
	end_wrap->setStyleClass("kb-editor-field-wrap");
	end_wrap->addNew<Wt::WText>("End date", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	auto* end_row = end_wrap->addNew<Wt::WContainerWidget>();
	end_row->setStyleClass("kb-date-row");
	m_end_date = end_row->addNew<Wt::WDateEdit>();
	m_end_date->setFormat("yyyy-MM-dd");
	m_end_date->setStyleClass("editor-field");
	m_end_date->changed().connect([] {});
	if(existing && existing->end_date.isValid())
	{
		m_end_date->setDate(existing->end_date);
	}
	auto* clear_end = end_row->addNew<Wt::WPushButton>("Clear");
	clear_end->setStyleClass("kb-date-clear");
	clear_end->clicked().connect([this] { m_end_date->setText(Wt::WString{}); });

	// ── Type ─────────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Type</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* type_row = form->addNew<Wt::WContainerWidget>();
	type_row->setStyleClass("kb-type-chips");

	m_type_id = 0;
	if(existing)
	{
		const bool valid_type = std::any_of(
		  types.begin(), types.end(), [&](const task_type_entry& ty) { return ty.id == existing->type_id; });
		if(valid_type)
		{
			m_type_id = existing->type_id;
		}
	}

	auto add_chip = [&](long long          chip_type_id,
	                    const std::string& label,
	                    const std::string& hex) {
		auto* chip = type_row->addNew<Wt::WContainerWidget>();
		chip->setStyleClass(chip_type_id == m_type_id ? "kb-type-chip selected" : "kb-type-chip");

		auto* dot = chip->addNew<Wt::WContainerWidget>();
		dot->setStyleClass("kb-type-chip__dot");
		if(hex.size() == 7 && hex[0] == '#')
		{
			try
			{
				int r = std::stoi(hex.substr(1, 2), nullptr, 16);
				int g = std::stoi(hex.substr(3, 2), nullptr, 16);
				int b = std::stoi(hex.substr(5, 2), nullptr, 16);
				dot->decorationStyle().setBackgroundColor(Wt::WColor{r, g, b});
			}
			catch(...)
			{}
		}
		chip->addNew<Wt::WText>(label, Wt::TextFormat::Plain);
		m_type_chips.push_back(chip);

		chip->clicked().connect([this, chip, chip_type_id] {
			m_type_id = chip_type_id;
			for(auto* c: m_type_chips)
			{
				c->removeStyleClass("selected");
			}
			chip->addStyleClass("selected");
		});
	};

	add_chip(0, "(None)", "#cccccc");
	for(const auto& ty: types)
	{
		add_chip(ty.id, ty.name, ty.color);
	}

	// ── Actions ───────────────────────────────────────────────────────────────
	m_status_msg = form->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status_msg->setStyleClass("editor-status");

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	m_save_btn =
	  btn_row->addNew<Wt::WPushButton>(is_new ? "Create Task" : "Save Changes");
	m_save_btn->setStyleClass("editor-btn");
	m_save_btn->clicked().connect([this] { save(); });

	// Delete only for leads editing existing tasks.
	if(!is_new && is_lead)
	{
		m_del_btn = btn_row->addNew<Wt::WPushButton>("Delete");
		m_del_btn->setStyleClass("editor-btn editor-btn-danger");
		m_del_btn->clicked().connect(
		  [this] {
			  const long long tid = m_existing->id;
			  if(m_task_id != 0)
			  {
				  live_hub::instance().unsubscribe(
				    "task:" + std::to_string(m_task_id), m_session_id);
				  m_task_id = 0;
			  }
			  m_db.delete_task(tid);
			  live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
			  live_hub::instance().broadcast("task:" + std::to_string(tid));
			  m_on_save();
		  });
	}

	const std::string back_url = "/board/" + std::to_string(team_id);
	btn_row->addNew<Wt::WAnchor>(
	         Wt::WLink{Wt::LinkType::InternalPath, back_url}, "Cancel")
	  ->setStyleClass("editor-btn editor-btn-cancel");

	if(existing)
	{
		m_task_id    = existing->id;
		m_session_id = Wt::WApplication::instance()->sessionId();
		live_hub::instance().subscribe(
		  "task:" + std::to_string(m_task_id),
		  m_session_id,
		  [this, alive = m_alive] {
			  if(*alive)
			  {
				  mark_stale();
				  Wt::WApplication::instance()->triggerUpdate();
			  }
		  });
	}
}

kanban_task_editor_page::~kanban_task_editor_page()
{
	*m_alive = false;
	if(m_task_id != 0)
	{
		live_hub::instance().unsubscribe(
		  "task:" + std::to_string(m_task_id), m_session_id);
	}
}

void kanban_task_editor_page::mark_stale()
{
	m_status_msg->setText(
	  "This task was modified by another user \xe2\x80\x94 saving is disabled.");
	if(m_save_btn)
	{
		m_save_btn->setDisabled(true);
	}
	if(m_del_btn)
	{
		m_del_btn->setDisabled(true);
	}
}

void kanban_task_editor_page::save()
{
	const std::string title = m_title->text().toUTF8();
	if(title.empty())
	{
		m_status_msg->setText("Title is required.");
		return;
	}

	const int         si     = m_status->currentIndex();
	const std::string status = (si >= 0 && si < static_cast<int>(k_status_vals.size())) ? k_status_vals[si] : "todo";

	const int         ai = m_assigned_to->currentIndex();
	const std::string new_assignee =
	  (ai >= 0 && ai < static_cast<int>(m_assignee_values.size())) ? m_assignee_values[ai] : "";

	// Assignment lock — enforced server-side even if the UI disables the widget.
	const std::string old_assignee = m_existing ? m_existing->assigned_to : "";
	if(!m_is_lead && new_assignee != old_assignee)
	{
		// A non-lead may only unassign themselves; they cannot assign to others.
		if(new_assignee != "" && new_assignee != m_username)
		{
			m_status_msg->setText("You cannot assign this task to another user.");
			return;
		}
		if(!old_assignee.empty() && old_assignee != m_username)
		{
			m_status_msg->setText("This task is already assigned to someone else.");
			return;
		}
	}

	kanban_task_entry t;
	t.team_id     = m_team_id;
	t.status      = status;
	t.title       = title;
	t.description = m_description->text().toUTF8();
	t.assigned_to = new_assignee;
	t.type_id     = m_type_id;
	if(const auto d = m_start_date->date(); d.isValid())
	{
		t.start_date = d;
	}
	if(const auto d = m_end_date->date(); d.isValid())
	{
		t.end_date = d;
	}

	if(m_existing)
	{
		t.id         = m_existing->id;
		t.sort_order = m_existing->sort_order;
		try
		{
			m_db.update_task(t);
		}
		catch(const Wt::Dbo::StaleObjectException&)
		{
			mark_stale();
			return;
		}
	}
	else
	{
		t.id = m_db.add_task(t);
	}

	// Fire a task_assigned notification if the assignee changed to a new person.
	if(!new_assignee.empty() && new_assignee != old_assignee &&
	   new_assignee != m_username)
	{
		const auto team = m_db.find_team(m_team_id);
		m_odb.push_notification(
		  new_assignee,
		  "task_assigned",
		  make_task_assigned_payload(t.id, title, m_team_id, team ? team->name : ""));
		live_hub::instance().broadcast("user:" + new_assignee);
	}

	if(m_task_id != 0)
	{
		live_hub::instance().unsubscribe(
		  "task:" + std::to_string(m_task_id), m_session_id);
		m_task_id = 0;
	}

	live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
	if(m_existing)
	{
		live_hub::instance().broadcast("task:" + std::to_string(m_existing->id));
	}

	m_on_save();
}
