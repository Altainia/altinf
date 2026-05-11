#include "kanban_task_editor_page.hpp"

#include <Wt/Dbo/Exception.h>
#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WColor.h>
#include <Wt/WDate.h>
#include <Wt/WDialog.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WTabWidget.h>
#include <Wt/WText.h>
#include <cmark.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "org/org.hpp"
#include "widgets/live_hub.hpp"

static std::string format_ts(const std::string& iso)
{
	if(iso.size() < 16)
	{
		return iso;
	}
	try
	{
		const int          yr       = std::stoi(iso.substr(0, 4));
		const int          mo       = std::stoi(iso.substr(5, 2));
		const int          dy       = std::stoi(iso.substr(8, 2));
		const auto         hr       = iso.substr(11, 2);
		const auto         mn       = iso.substr(14, 2);
		static const char* months[] = {
		  "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		return std::string(months[mo]) + " " + std::to_string(dy) +
		       ", " + std::to_string(yr) + " at " + hr + ":" + mn;
	}
	catch(...)
	{
		return iso;
	}
}

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
  team_cap::flags                     caps,
  const kanban_task_entry*            existing,
  const std::vector<std::string>&     members,
  const std::vector<task_type_entry>& types,
  std::function<void()>               on_save):
  m_db{db},
  m_odb{odb},
  m_team_id{team_id},
  m_username{session.username},
  m_caps{caps},
  m_existing{existing},
  m_on_save{std::move(on_save)}
{
	setStyleClass("page kb-editor-page");

	const bool        is_new           = (existing == nullptr);
	const std::string current_assignee = existing ? existing->assigned_to : "";
	const bool        can_edit         = caps.has_any(team_cap::edit_task_fields) && (!existing || !existing->is_archived);
	const bool        can_assign       = caps.has_any(team_cap::reassign_task) && (!existing || !existing->is_archived);
	const bool        locked_out       = !can_assign && !current_assignee.empty() && current_assignee != session.username;

	addNew<Wt::WText>(
	  is_new ? "<h1>New Task</h1>" : "<h1>Edit Task</h1>",
	  Wt::TextFormat::UnsafeXHTML);

	auto* tabs = addNew<Wt::WTabWidget>();
	tabs->setStyleClass("kb-editor-tabs");

	auto* details_panel = new Wt::WContainerWidget();
	tabs->addTab(std::unique_ptr<Wt::WContainerWidget>(details_panel), "Details");

	auto* form = details_panel;
	form->setStyleClass("kb-editor-form");

	m_history_panel = new Wt::WContainerWidget();
	tabs->addTab(std::unique_ptr<Wt::WContainerWidget>(m_history_panel), "History");

	tabs->currentChanged().connect([this](int index) {
		if(index == 1)
		{
			rebuild_history();
		}
	});

	// ── Title ─────────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Task</h2>", Wt::TextFormat::UnsafeXHTML);

	m_title = form->addNew<Wt::WLineEdit>();
	m_title->setPlaceholderText("Task title");
	m_title->setStyleClass("editor-field");
	if(existing)
	{
		m_title->setText(existing->title);
	}
	m_title->setReadOnly(!can_edit && existing != nullptr);

	m_description = form->addNew<Wt::WTextArea>();
	m_description->setPlaceholderText("Description (optional)");
	m_description->setStyleClass("editor-field kb-desc-field");
	if(existing)
	{
		m_description->setText(existing->description);
	}
	m_description->setReadOnly(!can_edit);

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
	m_status->setDisabled(!can_edit);

	auto* assignee_wrap = row->addNew<Wt::WContainerWidget>();
	assignee_wrap->setStyleClass("kb-editor-field-wrap");
	assignee_wrap->addNew<Wt::WText>("Assigned to", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	m_assigned_to = assignee_wrap->addNew<Wt::WComboBox>();
	m_assigned_to->setStyleClass("editor-field");

	// Build the assignee dropdown based on permission level.
	m_assignee_values.push_back(""); // index 0 = unassigned
	m_assigned_to->addItem("(unassigned)");

	if(can_assign)
	{
		for(const auto& m: members)
		{
			m_assignee_values.push_back(m);
			m_assigned_to->addItem(m);
		}
	}
	else
	{
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
	m_start_date->setReadOnly(!can_edit);
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
	m_end_date->setReadOnly(!can_edit);
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
	if(!can_edit)
	{
		for(auto* chip: m_type_chips)
		{
			chip->setDisabled(true);
		}
	}

	// ── Comments (inline, below type chips) ──────────────────────────────────
	if(existing)
	{
		auto* comment_section = form->addNew<Wt::WContainerWidget>();
		comment_section->setStyleClass("kb-comment-section");
		comment_section->addNew<Wt::WText>("<h2>Comments</h2>", Wt::TextFormat::UnsafeXHTML);
		m_comment_list = comment_section->addNew<Wt::WContainerWidget>();
		m_comment_list->setStyleClass("kb-comment-list");
		if(caps.has_any(team_cap::comment))
		{
			m_comment_compose = comment_section->addNew<Wt::WContainerWidget>();
			m_comment_compose->setStyleClass("kb-comment-compose");
		}
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

	if(!can_edit && !is_new)
	{
		m_save_btn->hide();
	}

	// Archive/Unarchive only for users with archive_task capability.
	if(!is_new && caps.has_any(team_cap::archive_task))
	{
		if(existing && existing->is_archived)
		{
			m_del_btn = btn_row->addNew<Wt::WPushButton>("Unarchive");
			m_del_btn->setStyleClass("editor-btn");
			m_del_btn->clicked().connect([this] {
				m_db.unarchive_task(m_existing->id, m_username);
				live_hub::instance().broadcast(
				  "team:" + std::to_string(m_team_id));
				live_hub::instance().broadcast(
				  "task:" + std::to_string(m_existing->id));
				m_on_save();
			});
		}
		else
		{
			m_del_btn = btn_row->addNew<Wt::WPushButton>("Archive");
			m_del_btn->setStyleClass("editor-btn editor-btn-danger");
			m_del_btn->clicked().connect([this] {
				const long long tid = m_existing->id;
				if(m_task_id != 0)
				{
					live_hub::instance().unsubscribe(
					  "task:" + std::to_string(m_task_id), m_session_id);
					m_task_id = 0;
				}
				m_db.archive_task(tid, m_username);
				live_hub::instance().broadcast(
				  "team:" + std::to_string(m_team_id));
				live_hub::instance().broadcast("task:" + std::to_string(tid));
				m_on_save();
			});
		}
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
		live_hub::instance().subscribe(
		  "task:" + std::to_string(m_task_id) + ":comments",
		  m_session_id,
		  [this, alive = m_alive] {
			  if(*alive)
			  {
				  rebuild_comments();
				  Wt::WApplication::instance()->triggerUpdate();
			  }
		  });
		rebuild_comments();
	}
}

kanban_task_editor_page::~kanban_task_editor_page()
{
	*m_alive = false;
	if(m_task_id != 0)
	{
		live_hub::instance().unsubscribe(
		  "task:" + std::to_string(m_task_id), m_session_id);
		live_hub::instance().unsubscribe(
		  "task:" + std::to_string(m_task_id) + ":comments", m_session_id);
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

void kanban_task_editor_page::rebuild_history()
{
	m_history_panel->clear();

	if(m_task_id == 0)
	{
		m_history_panel->addNew<Wt::WText>(
		  "No history yet.", Wt::TextFormat::Plain);
		return;
	}

	const auto events = m_db.history_for_task(m_task_id);
	if(events.empty())
	{
		m_history_panel->addNew<Wt::WText>(
		  "No history yet.", Wt::TextFormat::Plain);
		return;
	}

	static const std::map<std::string, std::string> k_field_labels = {
	  {"status", "Status"},
	  {"title", "Title"},
	  {"description", "Description"},
	  {"assigned_to", "Assigned to"},
	  {"start_date", "Start date"},
	  {"end_date", "End date"},
	  {"type", "Type"},
	};

	static const std::map<std::string, std::string> k_status_labels = {
	  {"todo", "To Do"},
	  {"in_progress", "In Progress"},
	  {"review", "Review"},
	  {"done", "Done"},
	};

	auto display_val = [&](const std::string& field,
	                       const std::string& val) -> std::string {
		if(val.empty())
		{
			return "(unset)";
		}
		if(field == "status")
		{
			const auto it = k_status_labels.find(val);
			return it != k_status_labels.end() ? it->second : val;
		}
		return val;
	};

	for(const auto& ev: events)
	{
		auto* entry = m_history_panel->addNew<Wt::WContainerWidget>();
		entry->setStyleClass("kb-history-entry");

		const std::string header =
		  format_ts(ev.occurred_at) + "  \xe2\x80\x94  " + ev.actor;
		auto* hdr = entry->addNew<Wt::WText>(header, Wt::TextFormat::Plain);
		hdr->setStyleClass("kb-history-header");

		if(ev.event_type == "created")
		{
			entry->addNew<Wt::WText>("[Task created]", Wt::TextFormat::Plain)
			  ->setStyleClass("kb-history-line");
		}
		else if(ev.event_type == "archived")
		{
			entry->addNew<Wt::WText>("[Task archived]", Wt::TextFormat::Plain)
			  ->setStyleClass("kb-history-line");
		}
		else if(ev.event_type == "unarchived")
		{
			entry->addNew<Wt::WText>("[Task unarchived]", Wt::TextFormat::Plain)
			  ->setStyleClass("kb-history-line");
		}
		else
		{
			for(const auto& ch: ev.changes)
			{
				const auto        label = k_field_labels.count(ch.field_name) ? k_field_labels.at(ch.field_name) : ch.field_name;
				const std::string line =
				  label + ": " +
				  display_val(ch.field_name, ch.old_value) +
				  " \xe2\x86\x92 " +
				  display_val(ch.field_name, ch.new_value);
				entry->addNew<Wt::WText>(line, Wt::TextFormat::Plain)
				  ->setStyleClass("kb-history-line");
			}
		}
	}
}

void kanban_task_editor_page::rebuild_comments()
{
	if(!m_comment_list)
	{
		return;
	}
	m_comment_list->clear();
	if(m_comment_compose)
	{
		m_comment_compose->clear();
	}

	if(m_task_id == 0)
	{
		m_comment_list->addNew<Wt::WText>(
		  "Save the task first to add comments.", Wt::TextFormat::Plain);
		return;
	}

	const auto comments = m_db.comments_for_task(m_task_id);

	if(comments.empty())
	{
		auto* empty = m_comment_list->addNew<Wt::WText>(
		  "No comments yet.", Wt::TextFormat::Plain);
		empty->setStyleClass("kb-comment-deleted");
	}
	else
	{
		for(const auto& c: comments)
		{
			auto* item = m_comment_list->addNew<Wt::WContainerWidget>();
			item->setStyleClass("kb-comment-item");

			if(c.is_deleted)
			{
				const std::string msg =
				  "Comment deleted by " + c.deleted_by +
				  " \xe2\x80\x94 " + format_ts(c.deleted_at);
				item->setStyleClass("kb-comment-item kb-comment-deleted");
				item->addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
				continue;
			}

			// Header: author + timestamp
			auto* hdr = item->addNew<Wt::WContainerWidget>();
			hdr->setStyleClass("kb-comment-header");
			auto* author_span = hdr->addNew<Wt::WText>(
			  c.author, Wt::TextFormat::Plain);
			author_span->setStyleClass("kb-comment-author");
			hdr->addNew<Wt::WText>(
			  " \xe2\x80\x94 " + format_ts(c.created_at), Wt::TextFormat::Plain);

			// Body (Markdown rendered)
			auto* body_wrap = item->addNew<Wt::WContainerWidget>();
			body_wrap->setStyleClass("kb-comment-body");
			char* html_raw = cmark_markdown_to_html(
			  c.body.c_str(), c.body.size(), CMARK_OPT_DEFAULT);
			const std::string body_html = html_raw ? std::string(html_raw) : "";
			if(html_raw)
			{
				free(html_raw);
			}
			body_wrap->addNew<Wt::WText>(body_html, Wt::TextFormat::UnsafeXHTML);

			if(!c.last_edited_at.empty())
			{
				auto* meta = item->addNew<Wt::WText>(
				  "Edited by " + c.last_edited_by +
				    " at " + format_ts(c.last_edited_at),
				  Wt::TextFormat::Plain);
				meta->setStyleClass("kb-comment-meta");
			}

			const bool can_act = m_caps.has_any(team_cap::comment) &&
			                     ((c.author == m_username) ||
			                      m_caps.has_any(team_cap::manage_team));
			if(can_act)
			{
				auto* actions = item->addNew<Wt::WContainerWidget>();
				actions->setStyleClass("kb-comment-actions");

				// Edit button + inline edit area (hidden until clicked)
				auto* edit_btn = actions->addNew<Wt::WPushButton>("Edit");
				auto* del_btn  = actions->addNew<Wt::WPushButton>("Delete");
				del_btn->setStyleClass("kb-comment-del-btn");

				const long long   cid      = c.id;
				const std::string c_author = c.author;
				const bool        is_own   = (c.author == m_username);

				// Inline edit area (hidden by default)
				auto* edit_area = item->addNew<Wt::WContainerWidget>();
				edit_area->setStyleClass("kb-comment-edit-area");
				edit_area->hide();
				auto* edit_ta = edit_area->addNew<Wt::WTextArea>();
				edit_ta->setText(c.body);
				edit_ta->setStyleClass("editor-field");
				auto* edit_btns = edit_area->addNew<Wt::WContainerWidget>();
				edit_btns->setStyleClass("kb-comment-edit-btns");
				auto* save_edit_btn = edit_btns->addNew<Wt::WPushButton>("Save");
				save_edit_btn->setStyleClass("editor-btn");
				auto* cancel_edit_btn = edit_btns->addNew<Wt::WPushButton>("Cancel");
				cancel_edit_btn->setStyleClass("editor-btn editor-btn-cancel");

				edit_btn->clicked().connect(
				  [this, c_author, is_own, body_wrap, edit_area, actions] {
					  if(is_own)
					  {
						  body_wrap->hide();
						  actions->hide();
						  edit_area->show();
					  }
					  else
					  {
						  auto* d = new Wt::WDialog("Edit Another User's Comment");
						  d->contents()->addNew<Wt::WText>(
						    "This comment was written by " + c_author +
						      ". Are you sure you want to edit it?",
						    Wt::TextFormat::Plain);
						  auto* yes = d->footer()->addNew<Wt::WPushButton>("Edit Anyway");
						  yes->setStyleClass("editor-btn");
						  auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
						  no->setStyleClass("editor-btn editor-btn-cancel");
						  yes->clicked().connect([d, body_wrap, edit_area, actions] {
							  d->accept();
							  body_wrap->hide();
							  actions->hide();
							  edit_area->show();
						  });
						  no->clicked().connect([d] { d->reject(); });
						  d->finished().connect([d](Wt::DialogCode) { delete d; });
						  d->show();
					  }
				  });

				cancel_edit_btn->clicked().connect([body_wrap, edit_area, actions] {
					edit_area->hide();
					body_wrap->show();
					actions->show();
				});

				save_edit_btn->clicked().connect(
				  [this, cid, edit_ta, save_edit_btn, alive = m_alive] {
					  if(!*alive)
					  {
						  return;
					  }
					  const std::string new_body = edit_ta->text().toUTF8();
					  if(new_body.empty())
					  {
						  return;
					  }
					  save_edit_btn->setDisabled(true);
					  m_db.edit_comment(cid, m_username, new_body);
					  live_hub::instance().broadcast(
					    "task:" + std::to_string(m_task_id) + ":comments");
					  rebuild_comments();
				  });

				del_btn->clicked().connect(
				  [this, cid, c_author, is_own] {
					  auto*             d = new Wt::WDialog("Delete Comment");
					  const std::string msg =
					    is_own ? "Are you sure you want to delete this comment?" : "This comment was written by " + c_author + ". Are you sure you want to delete it?";
					  d->contents()->addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
					  auto* yes = d->footer()->addNew<Wt::WPushButton>("Delete");
					  yes->setStyleClass("editor-btn editor-btn-danger");
					  auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
					  no->setStyleClass("editor-btn editor-btn-cancel");
					  yes->clicked().connect([this, d, cid, alive = m_alive] {
						  d->accept();
						  if(!*alive)
						  {
							  return;
						  }
						  m_db.delete_comment(cid, m_username);
						  live_hub::instance().broadcast(
						    "task:" + std::to_string(m_task_id) + ":comments");
						  rebuild_comments();
					  });
					  no->clicked().connect([d] { d->reject(); });
					  d->finished().connect([d](Wt::DialogCode) { delete d; });
					  d->show();
				  });
			}
		}
	}

	if(!m_comment_compose || (m_existing && m_existing->is_archived))
	{
		return;
	}

	// Compose box
	auto* ta = m_comment_compose->addNew<Wt::WTextArea>();
	ta->setPlaceholderText("Write a comment (Markdown supported)");
	ta->setStyleClass("editor-field");

	auto* post_btn = m_comment_compose->addNew<Wt::WPushButton>("Post Comment");
	post_btn->setStyleClass("editor-btn kb-comment-post-btn");

	post_btn->setDisabled(true);
	ta->keyWentUp().connect([ta, post_btn] {
		post_btn->setDisabled(ta->text().empty());
	});

	post_btn->clicked().connect(
	  [this, ta, post_btn, alive = m_alive] {
		  if(!*alive)
		  {
			  return;
		  }
		  const std::string body = ta->text().toUTF8();
		  if(body.empty())
		  {
			  return;
		  }
		  post_btn->setDisabled(true);
		  m_db.add_comment(m_task_id, m_username, body);
		  live_hub::instance().broadcast(
		    "task:" + std::to_string(m_task_id) + ":comments");
		  ta->setText(Wt::WString{});
		  rebuild_comments();
	  });
}

void kanban_task_editor_page::save()
{
	if(m_existing &&
	   (!m_caps.has_any(team_cap::edit_task_fields) || m_existing->is_archived))
	{
		m_status_msg->setText("You do not have permission to edit this task.");
		return;
	}

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
	if(!m_caps.has_any(team_cap::reassign_task) && new_assignee != old_assignee)
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
			m_db.update_task(t, m_username);
		}
		catch(const Wt::Dbo::StaleObjectException&)
		{
			mark_stale();
			return;
		}
	}
	else
	{
		t.id = m_db.add_task(t, m_username);
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
