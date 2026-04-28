#include "kanban_task_editor_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WColor.h>
#include <Wt/WDate.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>
#include <cstdio>

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

kanban_task_editor_page::kanban_task_editor_page(kanban_db&                      db,
                                                 long long                       team_id,
                                                 const session_data&             session,
                                                 const kanban_task_entry*        existing,
                                                 const std::vector<std::string>& members,
                                                 std::function<void()>           on_save):
  m_db{db},
  m_team_id{team_id},
  m_existing{existing},
  m_on_save{std::move(on_save)}
{
	setStyleClass("page kb-editor-page");

	const bool is_new = (existing == nullptr);
	addNew<Wt::WText>(
	  is_new ? "<h1>New Task</h1>" : "<h1>Edit Task</h1>", Wt::TextFormat::UnsafeXHTML);

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("kb-editor-form");

	// ── Title ─────────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Task</h2>", Wt::TextFormat::UnsafeXHTML);

	m_title = form->addNew<Wt::WLineEdit>();
	m_title->setPlaceholderText("Task title");
	m_title->setStyleClass("editor-field");
	if(existing)
		m_title->setText(existing->title);

	m_description = form->addNew<Wt::WTextArea>();
	m_description->setPlaceholderText("Description (optional)");
	m_description->setStyleClass("editor-field kb-desc-field");
	if(existing)
		m_description->setText(existing->description);

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
		m_status->addItem(lbl);
	if(existing)
	{
		const auto it =
		  std::find(k_status_vals.begin(), k_status_vals.end(), existing->status);
		if(it != k_status_vals.end())
			m_status->setCurrentIndex(
			  static_cast<int>(std::distance(k_status_vals.begin(), it)));
	}

	auto* assignee_wrap = row->addNew<Wt::WContainerWidget>();
	assignee_wrap->setStyleClass("kb-editor-field-wrap");
	assignee_wrap->addNew<Wt::WText>("Assigned to", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	m_assigned_to = assignee_wrap->addNew<Wt::WComboBox>();
	m_assigned_to->setStyleClass("editor-field");
	m_assigned_to->addItem("(unassigned)");
	for(const auto& m: members)
		m_assigned_to->addItem(m);
	if(existing && !existing->assigned_to.empty())
	{
		const auto it = std::find(members.begin(), members.end(), existing->assigned_to);
		if(it != members.end())
			m_assigned_to->setCurrentIndex(
			  static_cast<int>(std::distance(members.begin(), it)) + 1);
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
		m_start_date->setDate(existing->start_date);
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
		m_end_date->setDate(existing->end_date);
	auto* clear_end = end_row->addNew<Wt::WPushButton>("Clear");
	clear_end->setStyleClass("kb-date-clear");
	clear_end->clicked().connect([this] { m_end_date->setText(Wt::WString{}); });

	auto* color_wrap = sched_row->addNew<Wt::WContainerWidget>();
	color_wrap->setStyleClass("kb-editor-field-wrap");
	color_wrap->addNew<Wt::WText>("Color", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	m_color = color_wrap->addNew<Wt::WColorPicker>();
	m_color->setStyleClass("kb-color-picker");
	{
		const std::string hex = (existing && !existing->color.empty()) ? existing->color : "#7aa2d4";
		int               cr{0x7a}, cg{0xa2}, cb{0xd4};
		if(hex.size() == 7 && hex[0] == '#')
			std::sscanf(hex.c_str() + 1, "%02x%02x%02x", &cr, &cg, &cb);
		m_color->setColor(Wt::WColor(cr, cg, cb));
	}

	// ── Actions ───────────────────────────────────────────────────────────────
	m_status_msg = form->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status_msg->setStyleClass("editor-status");

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	auto* save_btn = btn_row->addNew<Wt::WPushButton>(is_new ? "Create Task" : "Save Changes");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect([this]() { save(); });

	if(!is_new)
	{
		auto* del_btn = btn_row->addNew<Wt::WPushButton>("Delete");
		del_btn->setStyleClass("editor-btn editor-btn-danger");
		del_btn->clicked().connect(
		  [this]() {
			  m_db.delete_task(m_existing->id);
			  m_on_save();
		  });
	}

	auto* cancel = btn_row->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/board"}, "Cancel");
	cancel->setStyleClass("editor-btn editor-btn-cancel");
}

void kanban_task_editor_page::save()
{
	const std::string title = m_title->text().toUTF8();
	if(title.empty())
	{
		m_status_msg->setText("Title is required.");
		return;
	}

	const int         si = m_status->currentIndex();
	const std::string status =
	  (si >= 0 && si < static_cast<int>(k_status_vals.size())) ? k_status_vals[si] : "todo";

	const int         ai = m_assigned_to->currentIndex();
	const std::string assignee =
	  (ai > 0 && ai <= static_cast<int>(m_assigned_to->count() - 1)) ? m_assigned_to->itemText(ai).toUTF8() : "";

	const auto wc = m_color->color();
	char       cbuf[8];
	std::snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x", wc.red(), wc.green(), wc.blue());

	kanban_task_entry t;
	t.team_id     = m_team_id;
	t.status      = status;
	t.title       = title;
	t.description = m_description->text().toUTF8();
	t.assigned_to = assignee;
	t.color       = cbuf;
	if(const auto d = m_start_date->date(); d.isValid())
		t.start_date = d;
	if(const auto d = m_end_date->date(); d.isValid())
		t.end_date = d;

	if(m_existing)
	{
		t.id         = m_existing->id;
		t.sort_order = m_existing->sort_order;
		m_db.update_task(t);
	}
	else
	{
		m_db.add_task(t);
	}

	m_on_save();
}
