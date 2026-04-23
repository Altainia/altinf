#include "gantt_editor_page.h"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <ctime>

namespace
{
	std::string today_string()
	{
		const std::time_t t  = std::time(nullptr);
		const std::tm*    tm = std::localtime(&t);
		char              buf[12];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
		return buf;
	}
} // namespace

gantt_editor_page::gantt_editor_page(gantt_db&                      db,
                                     const session_data&            session,
                                     const gantt_project_entry*     existing,
                                     std::vector<gantt_task_entry>  tasks,
                                     std::vector<std::string>       viewers,
                                     std::function<void(long long)> on_save):
  m_db{db},
  m_session{session},
  m_existing{existing},
  m_on_save{std::move(on_save)}
{
	setStyleClass("page gantt-editor-page");

	const bool is_new = (existing == nullptr);
	addNew<Wt::WText>(
	  is_new ? "<h1>New Chart</h1>" : "<h1>Edit Chart</h1>", Wt::TextFormat::UnsafeXHTML);

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("gantt-editor-form");

	// ── Metadata ──────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Details</h2>", Wt::TextFormat::UnsafeXHTML);

	m_title = form->addNew<Wt::WLineEdit>();
	m_title->setPlaceholderText("Chart title");
	m_title->setStyleClass("editor-field");
	if(existing)
		m_title->setText(existing->title);

	m_description = form->addNew<Wt::WTextArea>();
	m_description->setPlaceholderText("Description (optional)");
	m_description->setStyleClass("editor-field gantt-desc-field");
	if(existing)
		m_description->setText(existing->description);

	// ── Tasks ─────────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Tasks</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* task_col_hdr = form->addNew<Wt::WContainerWidget>();
	task_col_hdr->setStyleClass("gantt-task-col-hdr");
	task_col_hdr->addNew<Wt::WText>("Task", Wt::TextFormat::Plain)
	  ->setStyleClass("gantt-col-lbl gantt-col-title");
	task_col_hdr->addNew<Wt::WText>("Assigned to", Wt::TextFormat::Plain)
	  ->setStyleClass("gantt-col-lbl gantt-col-assigned");
	task_col_hdr->addNew<Wt::WText>("Start", Wt::TextFormat::Plain)
	  ->setStyleClass("gantt-col-lbl gantt-col-date");
	task_col_hdr->addNew<Wt::WText>("End", Wt::TextFormat::Plain)
	  ->setStyleClass("gantt-col-lbl gantt-col-date");
	task_col_hdr->addNew<Wt::WText>("Color", Wt::TextFormat::Plain)
	  ->setStyleClass("gantt-col-lbl gantt-col-color");

	m_tasks_container = form->addNew<Wt::WContainerWidget>();
	m_tasks_container->setStyleClass("gantt-tasks-container");

	for(const auto& t: tasks)
		add_task_row(t);

	auto* add_task_btn = form->addNew<Wt::WPushButton>("+ Add Task");
	add_task_btn->setStyleClass("editor-btn editor-btn-cancel gantt-add-btn");
	add_task_btn->clicked().connect([this]() {
		gantt_task_entry blank;
		blank.start_date = today_string();
		blank.end_date   = today_string();
		add_task_row(blank);
	});

	// ── Viewers ───────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Viewers</h2>", Wt::TextFormat::UnsafeXHTML);
	form->addNew<Wt::WText>(
	      "Grant view-only access by username. The owner always has full access.",
	      Wt::TextFormat::Plain)
	  ->setStyleClass("gantt-viewers-note");

	m_viewers_container = form->addNew<Wt::WContainerWidget>();
	m_viewers_container->setStyleClass("gantt-viewers-container");

	for(const auto& v: viewers)
		add_viewer_row(v);

	auto* viewer_add_row = form->addNew<Wt::WContainerWidget>();
	viewer_add_row->setStyleClass("gantt-viewer-add-row");
	m_viewer_input = viewer_add_row->addNew<Wt::WLineEdit>();
	m_viewer_input->setPlaceholderText("Username");
	m_viewer_input->setStyleClass("editor-field gantt-viewer-input");
	auto* add_viewer_btn = viewer_add_row->addNew<Wt::WPushButton>("Add Viewer");
	add_viewer_btn->setStyleClass("editor-btn editor-btn-cancel");
	add_viewer_btn->clicked().connect([this]() {
		const std::string u = m_viewer_input->text().toUTF8();
		if(u.empty())
			return;
		// Avoid duplicates
		for(const auto& r: m_viewer_rows)
			if(r.username == u)
				return;
		add_viewer_row(u);
		m_viewer_input->setText("");
	});

	// ── Save / Cancel ─────────────────────────────────────────────────────────
	m_status = form->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status->setStyleClass("editor-status");

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	auto* save_btn = btn_row->addNew<Wt::WPushButton>(is_new ? "Create Chart" : "Save Changes");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect([this]() { save(); });

	const std::string cancel_path =
	  existing ? ("/gantt/" + std::to_string(existing->id)) : "/gantt";
	auto* cancel = btn_row->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, cancel_path}, "Cancel");
	cancel->setStyleClass("editor-btn editor-btn-cancel");
}

void gantt_editor_page::add_task_row(const gantt_task_entry& task)
{
	auto* row = m_tasks_container->addNew<Wt::WContainerWidget>();
	row->setStyleClass("gantt-task-row");

	task_row r;
	r.container = row;

	r.title = row->addNew<Wt::WLineEdit>();
	r.title->setPlaceholderText("Task name");
	r.title->setStyleClass("editor-field gantt-col-title");
	r.title->setText(task.title);

	r.assigned_to = row->addNew<Wt::WLineEdit>();
	r.assigned_to->setPlaceholderText("Assignee");
	r.assigned_to->setStyleClass("editor-field gantt-col-assigned");
	r.assigned_to->setText(task.assigned_to);

	r.start_date = row->addNew<Wt::WLineEdit>();
	r.start_date->setPlaceholderText("YYYY-MM-DD");
	r.start_date->setStyleClass("editor-field gantt-col-date");
	r.start_date->setText(task.start_date);

	r.end_date = row->addNew<Wt::WLineEdit>();
	r.end_date->setPlaceholderText("YYYY-MM-DD");
	r.end_date->setStyleClass("editor-field gantt-col-date");
	r.end_date->setText(task.end_date);

	r.color = row->addNew<Wt::WLineEdit>();
	r.color->setPlaceholderText("#7aa2d4");
	r.color->setStyleClass("editor-field gantt-col-color");
	r.color->setText(task.color);

	auto* del_btn = row->addNew<Wt::WPushButton>("×");
	del_btn->setStyleClass("link-action-btn link-delete-btn gantt-del-btn");
	del_btn->clicked().connect([this, row]() { remove_task_row(row); });

	m_task_rows.push_back(std::move(r));
}

void gantt_editor_page::remove_task_row(Wt::WContainerWidget* c)
{
	m_task_rows.erase(
	  std::remove_if(m_task_rows.begin(), m_task_rows.end(), [c](const task_row& r) { return r.container == c; }),
	  m_task_rows.end());
	m_tasks_container->removeWidget(c);
}

void gantt_editor_page::add_viewer_row(const std::string& username)
{
	auto* row = m_viewers_container->addNew<Wt::WContainerWidget>();
	row->setStyleClass("gantt-viewer-row");

	viewer_row r;
	r.container = row;
	r.username  = username;

	row->addNew<Wt::WText>(username, Wt::TextFormat::Plain)->setStyleClass("gantt-viewer-name");

	auto* del_btn = row->addNew<Wt::WPushButton>("Remove");
	del_btn->setStyleClass("link-action-btn link-delete-btn");
	del_btn->clicked().connect([this, row]() { remove_viewer_row(row); });

	m_viewer_rows.push_back(std::move(r));
}

void gantt_editor_page::remove_viewer_row(Wt::WContainerWidget* c)
{
	m_viewer_rows.erase(
	  std::remove_if(m_viewer_rows.begin(), m_viewer_rows.end(), [c](const viewer_row& r) { return r.container == c; }),
	  m_viewer_rows.end());
	m_viewers_container->removeWidget(c);
}

void gantt_editor_page::save()
{
	const std::string title = m_title->text().toUTF8();
	if(title.empty())
	{
		m_status->setText("Title is required.");
		return;
	}

	gantt_project_entry proj;
	proj.title          = title;
	proj.description    = m_description->text().toUTF8();
	proj.owner_username = m_session.username;
	proj.created_date   = today_string();

	long long id = 0;
	if(m_existing)
	{
		proj.id             = m_existing->id;
		proj.owner_username = m_existing->owner_username;
		proj.created_date   = m_existing->created_date;
		m_db.update_project(proj);
		m_db.delete_tasks_for_project(proj.id);
		m_db.delete_viewers_for_project(proj.id);
		id = proj.id;
	}
	else
	{
		id = m_db.create_project(proj);
	}

	int sort = 0;
	for(const auto& r: m_task_rows)
	{
		gantt_task_entry t;
		t.project_id  = id;
		t.title       = r.title->text().toUTF8();
		t.assigned_to = r.assigned_to->text().toUTF8();
		t.start_date  = r.start_date->text().toUTF8();
		t.end_date    = r.end_date->text().toUTF8();
		t.color       = r.color->text().toUTF8();
		t.sort_order  = sort++;
		if(!t.title.empty())
			m_db.add_task(t);
	}

	for(const auto& vr: m_viewer_rows)
		if(!vr.username.empty())
			m_db.add_viewer(id, vr.username);

	m_on_save(id);
}
