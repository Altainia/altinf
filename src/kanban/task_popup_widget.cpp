#include "task_popup_widget.hpp"

#include <Wt/Dbo/Exception.h>
#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WDate.h>
#include <Wt/WDialog.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>
#include <cmark.h>

#include <algorithm>
#include <cstdlib>
#include <map>

#include "org/org.hpp"
#include "widgets/live_hub.hpp"

static std::string fmt_ts(const std::string& iso)
{
	if(iso.size() < 16)
	{
		return iso;
	}
	try
	{
		int                yr       = std::stoi(iso.substr(0, 4));
		int                mo       = std::stoi(iso.substr(5, 2));
		int                dy       = std::stoi(iso.substr(8, 2));
		static const char* months[] = {
		  "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		return std::string(months[mo]) + " " + std::to_string(dy) +
		       ", " + std::to_string(yr) + " at " + iso.substr(11, 2) + ":" + iso.substr(14, 2);
	}
	catch(...)
	{
		return iso;
	}
}

static std::string date_disp(const Wt::WDate& d)
{
	return d.isValid() ? d.toString("yyyy-MM-dd").toUTF8() : "(not set)";
}

static std::string status_lbl(const std::string& v)
{
	static const std::map<std::string, std::string> m = {
	  {"todo", "To Do"}, {"in_progress", "In Progress"}, {"review", "Review"}, {"done", "Done"}};
	auto it = m.find(v);
	return it != m.end() ? it->second : v;
}

const std::vector<std::string> task_popup_widget::k_status_vals   = {"todo", "in_progress", "review", "done"};
const std::vector<std::string> task_popup_widget::k_status_labels = {"To Do", "In Progress", "Review", "Done"};

task_popup_widget::task_popup_widget(kanban_db&                              db,
                                     org_db&                                 odb,
                                     long long                               task_id,
                                     const session_data&                     session,
                                     team_cap::flags                         caps,
                                     const std::map<long long, std::string>& type_colors,
                                     long long                               team_id):
  Wt::WDialog{},
  m_db{db},
  m_odb{odb},
  m_task_id{task_id},
  m_team_id{team_id},
  m_username{session.username},
  m_caps{caps},
  m_type_colors{type_colors}
{
	const auto task_opt = m_db.find_task(task_id);
	if(!task_opt)
	{
		setWindowTitle("Task not found");
		contents()->addNew<Wt::WText>("Task not found.", Wt::TextFormat::Plain);
		auto* close_btn = footer()->addNew<Wt::WPushButton>("Close");
		close_btn->setStyleClass("editor-btn editor-btn-cancel");
		close_btn->clicked().connect([this] { reject(); });
		finished().connect([this](Wt::DialogCode) { delete this; });
		addStyleClass("kb-task-popup");
		show();
		return;
	}
	m_original = *task_opt;

	const auto team_opt = m_db.find_team(team_id);
	m_org_id            = team_opt ? team_opt->org_id : 0;

	const bool can_edit   = caps.has_any(team_cap::edit_task_fields) && !m_original.is_archived;
	const bool can_assign = caps.has_any(team_cap::reassign_task) && !m_original.is_archived;

	setWindowTitle(m_original.title);
	addStyleClass("kb-task-popup");

	const std::string edit_url = "/board/" + std::to_string(team_id) +
	                             "/task/" + std::to_string(task_id) + "/edit";
	auto* full_link = titleBar()->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, edit_url},
	  "Open full editor \xe2\x86\x97");
	full_link->setStyleClass("kb-popup-full-link");
	full_link->clicked().connect([this] { reject(); });

	auto* body = contents();

	// ── Stale banner (hidden until a task:{id} broadcast arrives) ─────────────
	m_stale_banner = body->addNew<Wt::WContainerWidget>();
	m_stale_banner->setStyleClass("kb-popup-stale-banner");
	m_stale_banner->addNew<Wt::WText>(
	  "This task was updated by another user.", Wt::TextFormat::Plain);
	auto* reload_btn = m_stale_banner->addNew<Wt::WPushButton>("Reload");
	reload_btn->setStyleClass("editor-btn");
	reload_btn->clicked().connect([this] { reject(); });
	m_stale_banner->hide();

	// ── Fields ────────────────────────────────────────────────────────────────
	auto* fields = body->addNew<Wt::WContainerWidget>();
	fields->setStyleClass("kb-editor-form");

	// Title
	m_title_field = fields->addNew<Wt::WContainerWidget>();
	m_title_field->setStyleClass("kb-popup-field");
	m_title_field->addNew<Wt::WText>("Title", Wt::TextFormat::Plain)->setStyleClass("kb-field-label");
	m_title_display = m_title_field->addNew<Wt::WText>(m_original.title, Wt::TextFormat::Plain);
	m_title_display->setStyleClass(can_edit ? "kb-popup-display" : "");
	m_title_edit = m_title_field->addNew<Wt::WLineEdit>(m_original.title);
	m_title_edit->setStyleClass("editor-field");
	m_title_edit->hide();
	if(can_edit)
	{
		m_title_display->clicked().connect([this] { enter_edit_mode(m_title_display, m_title_edit); });
		m_title_edit->blurred().connect([this] {
			const std::string v = m_title_edit->text().toUTF8();
			exit_edit_mode(m_title_display, m_title_edit, v.empty() ? m_original.title : v);
			if(!v.empty() && v != m_original.title)
			{
				mark_field_dirty("title", m_title_field);
			}
		});
	}

	// Description
	m_desc_field = fields->addNew<Wt::WContainerWidget>();
	m_desc_field->setStyleClass("kb-popup-field");
	m_desc_field->addNew<Wt::WText>("Description", Wt::TextFormat::Plain)->setStyleClass("kb-field-label");
	m_desc_display = m_desc_field->addNew<Wt::WText>(
	  m_original.description.empty() ? "(none)" : m_original.description, Wt::TextFormat::Plain);
	m_desc_display->setStyleClass(can_edit ? "kb-popup-display" : "");
	m_desc_edit = m_desc_field->addNew<Wt::WTextArea>(m_original.description);
	m_desc_edit->setStyleClass("editor-field kb-desc-field");
	m_desc_edit->hide();
	if(can_edit)
	{
		m_desc_display->clicked().connect([this] { enter_edit_mode(m_desc_display, m_desc_edit); });
		m_desc_edit->blurred().connect([this] {
			const std::string v = m_desc_edit->text().toUTF8();
			exit_edit_mode(m_desc_display, m_desc_edit, v.empty() ? "(none)" : v);
			if(v != m_original.description)
			{
				mark_field_dirty("description", m_desc_field);
			}
		});
	}

	// Status + Assignee row
	auto* row = fields->addNew<Wt::WContainerWidget>();
	row->setStyleClass("kb-editor-row");

	m_status_field = row->addNew<Wt::WContainerWidget>();
	m_status_field->setStyleClass("kb-editor-field-wrap kb-popup-field");
	m_status_field->addNew<Wt::WText>("Status", Wt::TextFormat::Plain)->setStyleClass("kb-field-label");
	m_status_display = m_status_field->addNew<Wt::WText>(status_lbl(m_original.status), Wt::TextFormat::Plain);
	m_status_display->setStyleClass(can_edit ? "kb-popup-display" : "");
	m_status_edit = m_status_field->addNew<Wt::WComboBox>();
	m_status_edit->setStyleClass("editor-field");
	for(const auto& lbl: k_status_labels)
	{
		m_status_edit->addItem(lbl);
	}
	{
		const auto it = std::find(k_status_vals.begin(), k_status_vals.end(), m_original.status);
		if(it != k_status_vals.end())
		{
			m_status_edit->setCurrentIndex(static_cast<int>(std::distance(k_status_vals.begin(), it)));
		}
	}
	m_status_edit->hide();
	if(can_edit)
	{
		m_status_display->clicked().connect([this] { enter_edit_mode(m_status_display, m_status_edit); });
		m_status_edit->blurred().connect([this] {
			const int         si = m_status_edit->currentIndex();
			const std::string v  = (si >= 0 && si < static_cast<int>(k_status_vals.size())) ? k_status_vals[si] : "todo";
			exit_edit_mode(m_status_display, m_status_edit, status_lbl(v));
			if(v != m_original.status)
			{
				mark_field_dirty("status", m_status_field);
			}
		});
	}

	m_assignee_field = row->addNew<Wt::WContainerWidget>();
	m_assignee_field->setStyleClass("kb-editor-field-wrap kb-popup-field");
	m_assignee_field->addNew<Wt::WText>("Assigned to", Wt::TextFormat::Plain)->setStyleClass("kb-field-label");
	m_assignee_display = m_assignee_field->addNew<Wt::WText>(
	  m_original.assigned_to.empty() ? "(unassigned)" : m_original.assigned_to, Wt::TextFormat::Plain);
	const bool can_use_assignee = can_assign ||
	                              (!m_original.assigned_to.empty() && m_original.assigned_to == session.username);
	m_assignee_display->setStyleClass(can_use_assignee ? "kb-popup-display" : "");
	m_assignee_values.push_back("");
	m_assignee_edit = m_assignee_field->addNew<Wt::WComboBox>();
	m_assignee_edit->setStyleClass("editor-field");
	m_assignee_edit->addItem("(unassigned)");
	const auto members = m_db.members_for_team(team_id);
	if(can_assign)
	{
		for(const auto& mem: members)
		{
			m_assignee_values.push_back(mem);
			m_assignee_edit->addItem(mem);
		}
	}
	else
	{
		m_assignee_values.push_back(session.username);
		m_assignee_edit->addItem(session.username);
	}
	{
		const auto it = std::find(m_assignee_values.begin(), m_assignee_values.end(), m_original.assigned_to);
		if(it != m_assignee_values.end())
		{
			m_assignee_edit->setCurrentIndex(static_cast<int>(std::distance(m_assignee_values.begin(), it)));
		}
	}
	m_assignee_edit->hide();
	if(can_use_assignee)
	{
		m_assignee_display->clicked().connect([this] { enter_edit_mode(m_assignee_display, m_assignee_edit); });
		m_assignee_edit->blurred().connect([this] {
			const int         ai = m_assignee_edit->currentIndex();
			const std::string v  = (ai >= 0 && ai < static_cast<int>(m_assignee_values.size())) ? m_assignee_values[ai] : "";
			exit_edit_mode(m_assignee_display, m_assignee_edit, v.empty() ? "(unassigned)" : v);
			if(v != m_original.assigned_to)
			{
				mark_field_dirty("assigned_to", m_assignee_field);
			}
		});
	}

	// Dates — use a lambda to avoid repetition
	auto* sched_row = fields->addNew<Wt::WContainerWidget>();
	sched_row->setStyleClass("kb-editor-row");

	auto build_date_field = [&](Wt::WContainerWidget*  parent,
	                            const std::string&     label,
	                            const Wt::WDate&       orig,
	                            const std::string&     field_name,
	                            Wt::WText*&            disp_out,
	                            Wt::WDateEdit*&        edit_out,
	                            Wt::WContainerWidget*& wrap_out) {
		wrap_out = parent->addNew<Wt::WContainerWidget>();
		wrap_out->setStyleClass("kb-editor-field-wrap kb-popup-field");
		wrap_out->addNew<Wt::WText>(label, Wt::TextFormat::Plain)->setStyleClass("kb-field-label");
		disp_out = wrap_out->addNew<Wt::WText>(date_disp(orig), Wt::TextFormat::Plain);
		disp_out->setStyleClass(can_edit ? "kb-popup-display" : "");
		edit_out = wrap_out->addNew<Wt::WDateEdit>();
		edit_out->setFormat("yyyy-MM-dd");
		edit_out->setStyleClass("editor-field");
		edit_out->changed().connect([] {});
		if(orig.isValid())
		{
			edit_out->setDate(orig);
		}
		edit_out->hide();
		if(can_edit)
		{
			disp_out->clicked().connect([disp_out, edit_out, this] { enter_edit_mode(disp_out, edit_out); });
			edit_out->blurred().connect([this, orig, field_name, disp_out, edit_out, wrap_out] {
				const auto d = edit_out->date();
				exit_edit_mode(disp_out, edit_out, date_disp(d));
				if(d != orig)
				{
					mark_field_dirty(field_name, wrap_out);
				}
			});
		}
	};

	build_date_field(sched_row, "Start date", m_original.start_date, "start_date", m_start_date_display, m_start_date_edit, m_start_field);
	build_date_field(sched_row, "End date", m_original.end_date, "end_date", m_end_date_display, m_end_date_edit, m_end_field);

	// Type chips
	fields->addNew<Wt::WText>("<h2>Type</h2>", Wt::TextFormat::UnsafeXHTML);
	auto* type_row = fields->addNew<Wt::WContainerWidget>();
	type_row->setStyleClass("kb-type-chips");
	m_type_id        = 0;
	const auto types = m_db.types_for_org(m_org_id);
	{
		const bool valid = std::any_of(types.begin(), types.end(), [&](const task_type_entry& ty) { return ty.id == m_original.type_id; });
		if(valid)
		{
			m_type_id = m_original.type_id;
		}
	}
	auto add_chip = [&](long long chip_id, const std::string& label, const std::string& hex) {
		auto* chip = type_row->addNew<Wt::WContainerWidget>();
		chip->setStyleClass(chip_id == m_type_id ? "kb-type-chip selected" : "kb-type-chip");
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
		if(can_edit)
		{
			chip->clicked().connect([this, chip, chip_id] {
				m_type_id = chip_id;
				for(auto* c: m_type_chips)
				{
					c->removeStyleClass("selected");
				}
				chip->addStyleClass("selected");
				m_dirty_fields.insert("type");
				if(m_save_btn)
				{
					m_save_btn->setEnabled(true);
				}
			});
		}
	};
	add_chip(0, "(None)", "#cccccc");
	for(const auto& ty: types)
	{
		add_chip(ty.id, ty.name, ty.color);
	}

	// Comments section
	auto* comment_section = body->addNew<Wt::WContainerWidget>();
	comment_section->setStyleClass("kb-comment-section");
	comment_section->addNew<Wt::WText>("<h2>Comments</h2>", Wt::TextFormat::UnsafeXHTML);
	m_comment_list = comment_section->addNew<Wt::WContainerWidget>();
	m_comment_list->setStyleClass("kb-comment-list");
	if(caps.has_any(team_cap::comment) && !m_original.is_archived)
	{
		m_comment_compose = comment_section->addNew<Wt::WContainerWidget>();
		m_comment_compose->setStyleClass("kb-comment-compose");
	}

	// ── Footer ────────────────────────────────────────────────────────────────
	m_save_btn = footer()->addNew<Wt::WPushButton>("Save Changes");
	m_save_btn->setStyleClass("editor-btn");
	m_save_btn->setEnabled(false);
	m_save_btn->clicked().connect([this] { save(); });
	if(!can_edit)
	{
		m_save_btn->hide();
	}

	auto* close_btn = footer()->addNew<Wt::WPushButton>("Close");
	close_btn->setStyleClass("editor-btn editor-btn-cancel");
	close_btn->clicked().connect([this] { reject(); });

	finished().connect([this](Wt::DialogCode) { delete this; });

	// Live hub subscriptions
	m_session_id = Wt::WApplication::instance()->sessionId();
	live_hub::instance().subscribe(
	  "task:" + std::to_string(task_id), m_session_id, [this, alive = m_alive] {
		  if(*alive)
		  {
			  mark_stale();
			  Wt::WApplication::instance()->triggerUpdate();
		  }
	  });
	live_hub::instance().subscribe(
	  "task:" + std::to_string(task_id) + ":comments", m_session_id, [this, alive = m_alive] {
		  if(*alive)
		  {
			  rebuild_comments();
			  Wt::WApplication::instance()->triggerUpdate();
		  }
	  });
	rebuild_comments();
	show();
}

task_popup_widget::~task_popup_widget()
{
	*m_alive = false;
	if(m_task_id != 0)
	{
		live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
		live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id) + ":comments", m_session_id);
	}
}

void task_popup_widget::mark_stale()
{
	if(m_stale_banner)
	{
		m_stale_banner->show();
	}
	if(m_save_btn)
	{
		m_save_btn->setEnabled(false);
	}
}

void task_popup_widget::mark_field_dirty(const std::string& field, Wt::WContainerWidget* container)
{
	m_dirty_fields.insert(field);
	if(container)
	{
		container->addStyleClass("kb-popup-field--dirty");
	}
	if(m_save_btn)
	{
		m_save_btn->setEnabled(true);
	}
}

void task_popup_widget::enter_edit_mode(Wt::WText* display, Wt::WWidget* edit)
{
	display->hide();
	edit->show();
}

void task_popup_widget::exit_edit_mode(Wt::WText* display, Wt::WWidget* edit, const std::string& new_text)
{
	edit->hide();
	display->setText(new_text);
	display->show();
}

void task_popup_widget::rebuild_comments()
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

	const auto comments = m_db.comments_for_task(m_task_id);
	if(comments.empty())
	{
		m_comment_list->addNew<Wt::WText>("No comments yet.", Wt::TextFormat::Plain)
		  ->setStyleClass("kb-comment-deleted");
	}
	else
	{
		for(const auto& c: comments)
		{
			auto* item = m_comment_list->addNew<Wt::WContainerWidget>();
			item->setStyleClass("kb-comment-item");
			if(c.is_deleted)
			{
				item->setStyleClass("kb-comment-item kb-comment-deleted");
				item->addNew<Wt::WText>(
				  "Comment deleted by " + c.deleted_by + " \xe2\x80\x94 " + fmt_ts(c.deleted_at),
				  Wt::TextFormat::Plain);
				continue;
			}
			auto* hdr = item->addNew<Wt::WContainerWidget>();
			hdr->setStyleClass("kb-comment-header");
			hdr->addNew<Wt::WText>(c.author, Wt::TextFormat::Plain)->setStyleClass("kb-comment-author");
			hdr->addNew<Wt::WText>(" \xe2\x80\x94 " + fmt_ts(c.created_at), Wt::TextFormat::Plain);
			auto* body_wrap = item->addNew<Wt::WContainerWidget>();
			body_wrap->setStyleClass("kb-comment-body");
			char*             html_raw  = cmark_markdown_to_html(c.body.c_str(), c.body.size(), CMARK_OPT_DEFAULT);
			const std::string body_html = html_raw ? std::string(html_raw) : "";
			if(html_raw)
			{
				free(html_raw);
			}
			body_wrap->addNew<Wt::WText>(body_html, Wt::TextFormat::UnsafeXHTML);
			if(!c.last_edited_at.empty())
			{
				item->addNew<Wt::WText>(
				      "Edited by " + c.last_edited_by + " at " + fmt_ts(c.last_edited_at),
				      Wt::TextFormat::Plain)
				  ->setStyleClass("kb-comment-meta");
			}
			const bool can_act = m_caps.has_any(team_cap::comment) &&
			                     ((c.author == m_username) || m_caps.has_any(team_cap::manage_team));
			if(can_act)
			{
				auto* actions = item->addNew<Wt::WContainerWidget>();
				actions->setStyleClass("kb-comment-actions");
				auto* edit_btn = actions->addNew<Wt::WPushButton>("Edit");
				auto* del_btn  = actions->addNew<Wt::WPushButton>("Delete");
				del_btn->setStyleClass("kb-comment-del-btn");
				const long long   cid       = c.id;
				const std::string author    = c.author;
				const bool        is_own    = (c.author == m_username);
				auto*             edit_area = item->addNew<Wt::WContainerWidget>();
				edit_area->setStyleClass("kb-comment-edit-area");
				edit_area->hide();
				auto* edit_ta = edit_area->addNew<Wt::WTextArea>();
				edit_ta->setText(c.body);
				edit_ta->setStyleClass("editor-field");
				auto* edit_btns = edit_area->addNew<Wt::WContainerWidget>();
				edit_btns->setStyleClass("kb-comment-edit-btns");
				auto* save_edit = edit_btns->addNew<Wt::WPushButton>("Save");
				save_edit->setStyleClass("editor-btn");
				auto* cancel_edit = edit_btns->addNew<Wt::WPushButton>("Cancel");
				cancel_edit->setStyleClass("editor-btn editor-btn-cancel");
				edit_btn->clicked().connect([this, author, is_own, body_wrap, edit_area, actions] {
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
						  "This comment was written by " + author + ". Are you sure you want to edit it?",
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
				cancel_edit->clicked().connect([body_wrap, edit_area, actions] {
					edit_area->hide();
					body_wrap->show();
					actions->show();
				});
				save_edit->clicked().connect([this, cid, edit_ta, save_edit, alive = m_alive] {
					if(!*alive)
					{
						return;
					}
					const std::string nb = edit_ta->text().toUTF8();
					if(nb.empty())
					{
						return;
					}
					save_edit->setDisabled(true);
					m_db.edit_comment(cid, m_username, nb);
					live_hub::instance().broadcast("task:" + std::to_string(m_task_id) + ":comments");
					rebuild_comments();
				});
				del_btn->clicked().connect([this, cid, author, is_own] {
					auto*             d   = new Wt::WDialog("Delete Comment");
					const std::string msg = is_own ? "Are you sure you want to delete this comment?" : "This comment was written by " + author + ". Are you sure you want to delete it?";
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
						live_hub::instance().broadcast("task:" + std::to_string(m_task_id) + ":comments");
						rebuild_comments();
					});
					no->clicked().connect([d] { d->reject(); });
					d->finished().connect([d](Wt::DialogCode) { delete d; });
					d->show();
				});
			}
		}
	}

	if(!m_comment_compose || m_original.is_archived)
	{
		return;
	}
	auto* ta = m_comment_compose->addNew<Wt::WTextArea>();
	ta->setPlaceholderText("Write a comment (Markdown supported)");
	ta->setStyleClass("editor-field");
	auto* post_btn = m_comment_compose->addNew<Wt::WPushButton>("Post Comment");
	post_btn->setStyleClass("editor-btn kb-comment-post-btn");
	post_btn->setDisabled(true);
	ta->keyWentUp().connect([ta, post_btn] { post_btn->setDisabled(ta->text().empty()); });
	post_btn->clicked().connect([this, ta, post_btn, alive = m_alive] {
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
		live_hub::instance().broadcast("task:" + std::to_string(m_task_id) + ":comments");
		ta->setText(Wt::WString{});
		rebuild_comments();
	});
}

void task_popup_widget::save()
{
	if(!m_caps.has_any(team_cap::edit_task_fields) || m_original.is_archived)
	{
		return;
	}

	const std::string title = m_title_edit->text().toUTF8();
	if(title.empty())
	{
		return;
	}

	const int         si     = m_status_edit->currentIndex();
	const std::string status = (si >= 0 && si < static_cast<int>(k_status_vals.size())) ? k_status_vals[si] : m_original.status;

	const int         ai           = m_assignee_edit->currentIndex();
	const std::string new_assignee = (ai >= 0 && ai < static_cast<int>(m_assignee_values.size())) ? m_assignee_values[ai] : m_original.assigned_to;
	const std::string old_assignee = m_original.assigned_to;

	if(!m_caps.has_any(team_cap::reassign_task) && new_assignee != old_assignee)
	{
		if(!new_assignee.empty() && new_assignee != m_username)
		{
			return;
		}
		if(!old_assignee.empty() && old_assignee != m_username)
		{
			return;
		}
	}

	kanban_task_entry t;
	t.id          = m_original.id;
	t.team_id     = m_team_id;
	t.status      = status;
	t.title       = title;
	t.description = m_desc_edit->text().toUTF8();
	t.assigned_to = new_assignee;
	t.type_id     = m_type_id;
	t.sort_order  = m_original.sort_order;
	if(const auto d = m_start_date_edit->date(); d.isValid())
	{
		t.start_date = d;
	}
	if(const auto d = m_end_date_edit->date(); d.isValid())
	{
		t.end_date = d;
	}

	try
	{
		m_db.update_task(t, m_username);
	}
	catch(const Wt::Dbo::StaleObjectException&)
	{
		mark_stale();
		return;
	}

	if(!new_assignee.empty() && new_assignee != old_assignee && new_assignee != m_username)
	{
		const auto team = m_db.find_team(m_team_id);
		m_odb.push_notification(
		  new_assignee, "task_assigned", make_task_assigned_payload(t.id, title, m_team_id, team ? team->name : ""));
		live_hub::instance().broadcast("user:" + new_assignee);
	}

	live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
	m_task_id = 0;
	live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
	live_hub::instance().broadcast("task:" + std::to_string(m_original.id));
	accept();
}
