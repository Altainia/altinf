#include "task_editor_form_widget.hpp"

#include <Wt/Dbo/Exception.h>
#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WColor.h>
#include <Wt/WDate.h>
#include <Wt/WDialog.h>
#include <Wt/WLink.h>
#include <Wt/WTabWidget.h>
#include <Wt/WText.h>

#include <algorithm>
#include <alt/functional.hpp>
#include <cstdlib>
#include <map>

#include "org/kanban_notifications.hpp"
#include "org/org.hpp"
#include "widgets/live_hub.hpp"
#include "widgets/markdown_editor_widget.hpp"
#include "widgets/markdown_viewer_widget.hpp"

// ── Static helpers ────────────────────────────────────────────────────────────

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
		static const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		return std::string(months[mo]) + " " + std::to_string(dy) +
		       ", " + std::to_string(yr) +
		       " at " + iso.substr(11, 2) + ":" + iso.substr(14, 2);
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

const std::vector<std::string> task_editor_form_widget::k_status_vals =
  {"todo", "in_progress", "review", "done"};
const std::vector<std::string> task_editor_form_widget::k_status_labels =
  {"To Do", "In Progress", "Review", "Done"};

// ── Constructor ───────────────────────────────────────────────────────────────

task_editor_form_widget::task_editor_form_widget(
  kanban_db&          db,
  org_db&             odb,
  long long           task_id,
  long long           team_id,
  const session_data& session,
  team_cap::flags     caps,
  team_settings_entry settings): m_db{db},
                                 m_odb{odb},
                                 m_task_id{task_id},
                                 m_team_id{team_id},
                                 m_username{session.username},
                                 m_caps{caps},
                                 m_settings{settings}
{
	const bool is_new = (task_id == 0);

	if(!is_new)
	{
		const auto opt = m_db.find_task(task_id);
		if(!opt)
		{
			addNew<Wt::WText>("Task not found.", Wt::TextFormat::Plain);
			return;
		}
		m_original = *opt;
	}

	const auto team_opt = m_db.find_team(team_id);
	m_org_id            = team_opt ? team_opt->org_id : 0;

	const bool is_lead         = caps.has_any(team_cap::edit_task_details);
	const bool not_archived    = is_new || !m_original.is_archived;
	const bool can_edit        = is_lead && not_archived;
	const bool can_edit_status = not_archived &&
	                             (is_lead || (caps.has_any(team_cap::edit_task_fields) &&
	                                          m_settings.allow_member_move_columns));
	const bool can_reassign = caps.has_any(team_cap::reassign_task) && not_archived;

	// ── Stale banner ──────────────────────────────────────────────────────────
	if(!is_new)
	{
		m_stale_banner = addNew<Wt::WContainerWidget>();
		m_stale_banner->setStyleClass("kb-popup-stale-banner");
		m_stale_banner->addNew<Wt::WText>(
		  "This task was updated by another user.", Wt::TextFormat::Plain);
		auto* reload_btn = m_stale_banner->addNew<Wt::WPushButton>("Reload");
		reload_btn->setStyleClass("editor-btn");
		reload_btn->clicked().connect([this] { canceled.emit(); });
		m_stale_banner->hide();
	}

	// ── Tab widget (existing tasks) or plain container (new tasks) ────────────
	Wt::WContainerWidget* form = nullptr;
	if(!is_new)
	{
		auto* tabs = addNew<Wt::WTabWidget>();
		tabs->setStyleClass("kb-editor-tabs");
		auto* details = new Wt::WContainerWidget();
		details->setStyleClass("kb-editor-form");
		tabs->addTab(std::unique_ptr<Wt::WContainerWidget>(details), "Details");
		form            = details;
		m_history_panel = new Wt::WContainerWidget();
		tabs->addTab(std::unique_ptr<Wt::WContainerWidget>(m_history_panel), "History");
		tabs->currentChanged().connect([this](int idx) { if(idx==1){ rebuild_history();} });
	}
	else
	{
		setStyleClass("kb-editor-form");
		form = this;
	}

	// ── Title ─────────────────────────────────────────────────────────────────
	m_title_field = form->addNew<Wt::WContainerWidget>();
	m_title_field->setStyleClass("kb-popup-field kb-popup-title-field");
	m_title_field->addNew<Wt::WText>("Title", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");

	const std::string title_val = is_new ? "" : m_original.title;
	m_title_display             = m_title_field->addNew<Wt::WText>(title_val, Wt::TextFormat::Plain);
	m_title_display->setStyleClass(can_edit && !is_new ? "kb-popup-display" : "");
	if(is_new)
	{
		m_title_display->hide();
	}

	m_title_edit = m_title_field->addNew<Wt::WLineEdit>(title_val);
	m_title_edit->setPlaceholderText("Task title");
	m_title_edit->setStyleClass("editor-field");
	if(!is_new)
	{
		if(can_edit)
		{
			m_title_edit->hide();
		}
		else
		{
			// Read-only view: show the input as readonly so tests can locate it.
			m_title_edit->setReadOnly(true);
			m_title_display->hide();
		}
	}

	if(can_edit && !is_new)
	{
		m_title_display->clicked().connect([this] {
			enter_edit_mode(m_title_display, m_title_edit);
		});
		m_title_edit->blurred().connect([this] {
			const std::string v  = m_title_edit->text().toUTF8();
			const std::string dv = v.empty() ? m_original.title : v;
			if(v.empty())
			{
				m_title_edit->setText(m_original.title);
			}
			exit_edit_mode(m_title_display, m_title_edit, dv);
			if(dv == m_original.title)
			{
				unmark_field_dirty("title", m_title_field);
			}
			else
			{
				mark_field_dirty("title", m_title_field);
			}
		});
	}

	// ── Description ───────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Description</h2>", Wt::TextFormat::UnsafeXHTML);
	m_desc_field = form->addNew<Wt::WContainerWidget>();
	m_desc_field->setStyleClass("kb-popup-field");

	const std::string desc_val = is_new ? "" : m_original.description;

	m_desc_viewer = m_desc_field->addNew<markdown_viewer_widget>(desc_val);
	m_desc_viewer->setStyleClass("kb-desc-display");
	if(can_edit && !is_new)
	{
		m_desc_viewer->addStyleClass("kb-popup-display");
	}
	if(is_new)
	{
		m_desc_viewer->hide();
	}

	m_desc_editor = m_desc_field->addNew<markdown_editor_widget>(desc_val);
	m_desc_editor->setStyleClass("kb-desc-field");
	if(!is_new)
	{
		m_desc_editor->hide();
	}

	if(can_edit && !is_new)
	{
		m_desc_viewer->clicked().connect([this] {
			m_desc_viewer->hide();
			m_desc_editor->show();
			m_desc_editor->focus();
		});

		m_desc_editor->changed().connect([this](const std::string& v) {
			m_desc_editor->hide();
			m_desc_viewer->set_content(v);
			m_desc_viewer->show();
			if(v == m_original.description)
			{
				unmark_field_dirty("description", m_desc_field);
			}
			else
			{
				mark_field_dirty("description", m_desc_field);
			}
		});
	}

	// ── Status + Assignee row ─────────────────────────────────────────────────
	auto* row = form->addNew<Wt::WContainerWidget>();
	row->setStyleClass("kb-editor-row");

	// Status
	m_status_field = row->addNew<Wt::WContainerWidget>();
	m_status_field->setStyleClass("kb-editor-field-wrap kb-popup-field");
	m_status_field->addNew<Wt::WText>("Status", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");
	const std::string status_init = is_new ? "todo" : m_original.status;
	m_status_display              = m_status_field->addNew<Wt::WText>(
    status_lbl(status_init), Wt::TextFormat::Plain);
	m_status_display->setStyleClass(can_edit_status && !is_new ? "kb-popup-display" : "");
	if(is_new)
	{
		m_status_display->hide();
	}
	m_status_edit = m_status_field->addNew<Wt::WComboBox>();
	m_status_edit->setStyleClass("editor-field");
	for(size_t i = 0; i < k_status_vals.size(); ++i)
	{
		if(k_status_vals[i] == "done" && can_edit_status && !is_lead)
		{
			continue;
		}
		m_status_vals_used.push_back(k_status_vals[i]);
		m_status_edit->addItem(k_status_labels[i]);
	}
	{
		const auto it = std::find(
		  m_status_vals_used.begin(), m_status_vals_used.end(), status_init);
		if(it != m_status_vals_used.end())
		{
			m_status_edit->setCurrentIndex(
			  static_cast<int>(std::distance(m_status_vals_used.begin(), it)));
		}
	}
	if(!is_new)
	{
		if(can_edit_status)
		{
			m_status_edit->hide();
		}
		else
		{
			m_status_edit->setDisabled(true);
			m_status_display->hide();
		}
	}

	if(can_edit_status && !is_new)
	{
		m_status_display->clicked().connect([this] {
			enter_edit_mode(m_status_display, m_status_edit);
		});
		m_status_edit->changed().connect([this] {
			const int         si = m_status_edit->currentIndex();
			const std::string v =
			  (si >= 0 && si < static_cast<int>(m_status_vals_used.size())) ? m_status_vals_used[si] : m_original.status;
			exit_edit_mode(m_status_display, m_status_edit, status_lbl(v));
			if(v == m_original.status)
			{
				unmark_field_dirty("status", m_status_field);
			}
			else
			{
				mark_field_dirty("status", m_status_field);
			}
		});
		m_status_edit->blurred().connect([this] {
			if(m_status_edit->isHidden())
			{
				return;
			}
			const int         si = m_status_edit->currentIndex();
			const std::string v =
			  (si >= 0 && si < static_cast<int>(m_status_vals_used.size())) ? m_status_vals_used[si] : m_original.status;
			exit_edit_mode(m_status_display, m_status_edit, status_lbl(v));
			if(v == m_original.status)
			{
				unmark_field_dirty("status", m_status_field);
			}
			else
			{
				mark_field_dirty("status", m_status_field);
			}
		});
	}

	// ── Assignees ─────────────────────────────────────────────────────────────────
	m_assignee_list = row->addNew<Wt::WContainerWidget>();
	m_assignee_list->setStyleClass("kb-editor-field-wrap kb-popup-field");
	m_assignee_list->addNew<Wt::WText>("Assignees", Wt::TextFormat::Plain)
	  ->setStyleClass("kb-field-label");

	auto* chips_container = m_assignee_list->addNew<Wt::WContainerWidget>();
	chips_container->setStyleClass("kb-assignee-chips");

	if(is_new)
	{
		// New task: assignees are collected locally and written after the task is created.
		if(can_reassign)
		{
			auto rebuild_new_fn = std::make_shared<std::function<void()>>();
			*rebuild_new_fn     = [this, chips_container, rebuild_new_fn]() {
        chips_container->clear();
        for(const auto& user: m_pending_assignees)
        {
          auto* chip = chips_container->addNew<Wt::WContainerWidget>();
          chip->setStyleClass("kb-assignee-chip");
          chip->addNew<Wt::WText>(user, Wt::TextFormat::Plain);
          auto* rm = chip->addNew<Wt::WPushButton>("\xc3\x97");
          rm->setStyleClass("kb-assignee-rm");
          rm->clicked().connect([this, user, rebuild_new_fn]() {
            auto it = std::find(
              m_pending_assignees.begin(), m_pending_assignees.end(), user);
            if(it != m_pending_assignees.end())
            {
              m_pending_assignees.erase(it);
            }
            (*rebuild_new_fn)();
          });
        }
        if(m_add_member_combo)
        {
          m_add_member_combo->clear();
          m_add_member_combo->addItem("(select member)");
          const auto all = m_db.members_for_team(m_team_id);
          for(const auto& mem: all)
          {
            if(std::find(m_pending_assignees.begin(),
                         m_pending_assignees.end(),
                         mem) == m_pending_assignees.end())
            {
              m_add_member_combo->addItem(mem);
            }
          }
          m_add_member_combo->setCurrentIndex(0);
        }
			};

			auto* add_row = m_assignee_list->addNew<Wt::WContainerWidget>();
			add_row->setStyleClass("kb-assignee-add-row");
			m_add_member_combo = add_row->addNew<Wt::WComboBox>();
			m_add_member_combo->setStyleClass("editor-field");
			m_add_member_combo->addItem("(select member)");
			const auto new_team_members = m_db.members_for_team(m_team_id);
			for(const auto& mem: new_team_members)
			{
				m_add_member_combo->addItem(mem);
			}
			auto* add_btn = add_row->addNew<Wt::WPushButton>("Add");
			add_btn->setStyleClass("editor-btn");
			add_btn->clicked().connect([this, rebuild_new_fn]() {
				if(!m_add_member_combo || m_add_member_combo->currentIndex() <= 0)
				{
					return;
				}
				const std::string new_user = m_add_member_combo->currentText().toUTF8();
				if(new_user.empty())
				{
					return;
				}
				m_pending_assignees.push_back(new_user);
				(*rebuild_new_fn)();
			});
		}
	}
	else
	{
		auto rebuild_chips_fn = std::make_shared<std::function<void()>>();
		*rebuild_chips_fn     = [this, chips_container, can_reassign, not_archived, rebuild_chips_fn]() {
      chips_container->clear();
      const auto current = m_db.assignees_for_task(m_task_id);
      for(const auto& user: current)
      {
        auto* chip = chips_container->addNew<Wt::WContainerWidget>();
        chip->setStyleClass("kb-assignee-chip");
        chip->addNew<Wt::WText>(user, Wt::TextFormat::Plain);

        const bool is_self            = (user == m_username);
        const bool sole               = (current.size() == 1);
        const bool can_remove_as_lead = can_reassign;
        const bool can_remove_as_member =
          !can_reassign && is_self && not_archived &&
          (sole ? m_settings.allow_abandon : m_settings.allow_self_assign_assigned);

        if(can_remove_as_lead || can_remove_as_member)
        {
          auto* rm = chip->addNew<Wt::WPushButton>("\xc3\x97");
          rm->setStyleClass("kb-assignee-rm");
          rm->clicked().connect(
            [this, user, sole, can_reassign, rebuild_chips_fn]() {
              auto do_remove = [this, user, rebuild_chips_fn]() {
                m_db.remove_assignee(m_task_id, user, m_username);
                const auto remaining = m_db.assignees_for_task(m_task_id);
                notify_assignee_removed(
                  m_db, m_odb, m_task_id, m_team_id, m_org_id, user, m_username, remaining);
                live_hub::instance().broadcast(
                  "team:" + std::to_string(m_team_id));
                (*rebuild_chips_fn)();
              };
              if(can_reassign && sole && !m_settings.allow_abandon)
              {
                auto* dlg = addNew<Wt::WDialog>("Confirm abandon");
                dlg->contents()->addNew<Wt::WText>(
                  "This task has no other assignees. Removing " + user +
                    " will leave it unassigned. Proceed?",
                  Wt::TextFormat::Plain);
                auto* yes =
                  dlg->footer()->addNew<Wt::WPushButton>("Yes, abandon");
                yes->setStyleClass("editor-btn");
                auto* no = dlg->footer()->addNew<Wt::WPushButton>("Cancel");
                no->setStyleClass("editor-btn editor-btn-cancel");
                yes->clicked().connect([dlg, do_remove]() mutable {
                  dlg->reject();
                  do_remove();
                });
                no->clicked().connect([dlg]() { dlg->reject(); });
                dlg->show();
              }
              else
              {
                do_remove();
              }
            });
        }
      }
      // Rebuild combo to exclude already-assigned members (Bug 3 fix).
      if(m_add_member_combo)
      {
        m_add_member_combo->clear();
        m_add_member_combo->addItem("(select member)");
        const auto all = m_db.members_for_team(m_team_id);
        for(const auto& mem: all)
        {
          if(std::find(current.begin(), current.end(), mem) == current.end())
          {
            m_add_member_combo->addItem(mem);
          }
        }
        m_add_member_combo->setCurrentIndex(0);
      }
		};

		(*rebuild_chips_fn)();

		const bool already_assigned =
		  std::find(m_original.assignees.begin(), m_original.assignees.end(), m_username) !=
		  m_original.assignees.end();
		const bool can_self_add =
		  !can_reassign && not_archived && !already_assigned &&
		  (m_original.assignees.empty() ? m_settings.allow_self_assign_unassigned : m_settings.allow_self_assign_assigned);

		if(can_self_add)
		{
			auto* self_btn = m_assignee_list->addNew<Wt::WPushButton>("Assign to me");
			self_btn->setStyleClass("editor-btn");
			self_btn->clicked().connect([this, rebuild_chips_fn]() {
				if(m_db.add_assignee(m_task_id, m_username, m_username))
				{
					notify_assignee_added(
					  m_db, m_odb, m_task_id, m_team_id, m_org_id, m_username, m_username);
					live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
					(*rebuild_chips_fn)();
				}
			});
		}

		if(can_reassign)
		{
			auto* add_row = m_assignee_list->addNew<Wt::WContainerWidget>();
			add_row->setStyleClass("kb-assignee-add-row");
			m_add_member_combo = add_row->addNew<Wt::WComboBox>();
			m_add_member_combo->setStyleClass("editor-field");
			m_add_member_combo->addItem("(select member)");
			const auto team_members = m_db.members_for_team(m_team_id);
			for(const auto& mem: team_members)
			{
				m_add_member_combo->addItem(mem);
			}
			auto* add_btn = add_row->addNew<Wt::WPushButton>("Add");
			add_btn->setStyleClass("editor-btn");
			add_btn->clicked().connect([this, rebuild_chips_fn]() {
				if(!m_add_member_combo || m_add_member_combo->currentIndex() <= 0)
				{
					return;
				}
				const std::string new_user = m_add_member_combo->currentText().toUTF8();
				if(new_user.empty())
				{
					return;
				}
				if(m_db.add_assignee(m_task_id, new_user, m_username))
				{
					notify_assignee_added(
					  m_db, m_odb, m_task_id, m_team_id, m_org_id, new_user, m_username);
					live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
					(*rebuild_chips_fn)();
				}
			});
		}
	}

	// ── Date fields ───────────────────────────────────────────────────────────
	auto* sched_row = form->addNew<Wt::WContainerWidget>();
	sched_row->setStyleClass("kb-editor-row");

	auto build_date = [&](Wt::WContainerWidget*  parent,
	                      const std::string&     label,
	                      const Wt::WDate&       orig,
	                      const std::string&     field_name,
	                      Wt::WText*&            disp,
	                      Wt::WDateEdit*&        edit,
	                      Wt::WContainerWidget*& wrap) {
		wrap = parent->addNew<Wt::WContainerWidget>();
		wrap->setStyleClass("kb-editor-field-wrap kb-popup-field");
		wrap->addNew<Wt::WText>(label, Wt::TextFormat::Plain)->setStyleClass("kb-field-label");
		disp = wrap->addNew<Wt::WText>(date_disp(orig), Wt::TextFormat::Plain);
		disp->setStyleClass(can_edit && !is_new ? "kb-popup-display" : "");
		if(is_new)
		{
			disp->hide();
		}
		edit = wrap->addNew<Wt::WDateEdit>();
		edit->setFormat("yyyy-MM-dd");
		edit->setStyleClass("editor-field");
		if(orig.isValid())
		{
			edit->setDate(orig);
		}
		if(!is_new)
		{
			edit->hide();
		}
		if(can_edit && !is_new)
		{
			disp->clicked().connect([disp, edit, this] { enter_edit_mode(disp, edit); });

			// changed() fires after WDateEdit updates its text from the calendar
			// (setFromCalendar → changed().emit()), so edit->date() is the new value.
			// Also corrects the Safari case: if blurred() already exited with the old
			// date, changed() fixes the display text without re-entering edit mode.
			edit->changed().connect([this, orig, field_name, disp, edit, wrap] {
				const auto d   = edit->date();
				const auto txt = date_disp(d);
				if(disp->isHidden())
				{
					exit_edit_mode(disp, edit, txt);
				}
				else
				{
					disp->setText(txt);
				}
				if(d == orig)
				{
					unmark_field_dirty(field_name, wrap);
				}
				else
				{
					mark_field_dirty(field_name, wrap);
				}
			});

			// blurred() handles "click away without selecting a date".
			// Guard prevents double-exit when changed() already ran first.
			edit->blurred().connect([this, orig, field_name, disp, edit, wrap] {
				if(!disp->isHidden())
				{
					return;
				}
				const auto d = edit->date();
				exit_edit_mode(disp, edit, date_disp(d));
				if(d == orig)
				{
					unmark_field_dirty(field_name, wrap);
				}
				else
				{
					mark_field_dirty(field_name, wrap);
				}
			});

			// Prevent focus from leaving the WDateEdit when the user clicks
			// calendar navigation arrows or day cells (non-focusable elements
			// inside .Wt-datepicker).  Without this, mousedown on those elements
			// causes blur → blurred() → exit_edit_mode → WDateEdit::setHidden →
			// popup_->setHidden, closing the calendar the user is still using.
			// <select> elements (month picker) are excluded so their native
			// dropdown still opens; that edge case is an accepted limitation.
			doJavaScript(
			  "if(!window._kbDpBound){"
			  "  window._kbDpBound=true;"
			  "  document.addEventListener('mousedown',function(e){"
			  "    if(!e.target.closest)return;"
			  "    if(!e.target.closest('.Wt-datepicker'))return;"
			  "    if(e.target.tagName==='SELECT'||e.target.closest('select'))return;"
			  "    e.preventDefault();"
			  "  },true);"
			  "}");

			// ── Clear button ───────────────────────────────────────────────────
			auto* clear_btn = wrap->addNew<Wt::WPushButton>("\xc3\x97");
			clear_btn->setStyleClass("kb-date-clear");
			clear_btn->clicked().connect([this, orig, field_name, disp, edit, wrap] {
				// setText("") directly clears the underlying WLineEdit input value
				// (WDateEdit::setDate skips non-null check; setText always updates).
				edit->setText(Wt::WString{});
				// Enter edit mode so the empty input is visible in the DOM.
				enter_edit_mode(disp, edit);
				// Mark dirty only if the original date was set (we changed something).
				if(orig.isValid())
				{
					mark_field_dirty(field_name, wrap);
				}
				else
				{
					unmark_field_dirty(field_name, wrap);
				}
			});
		}
	};

	build_date(sched_row, "Start date", is_new ? Wt::WDate{} : m_original.start_date, "start_date", m_start_date_display, m_start_date_edit, m_start_field);
	build_date(sched_row, "End date", is_new ? Wt::WDate{} : m_original.end_date, "end_date", m_end_date_display, m_end_date_edit, m_end_field);

	// For new tasks, show date edits directly with default start = today
	if(is_new)
	{
		m_start_date_edit->setDate(Wt::WDate::currentDate());
	}

	// ── Type chips ────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Type</h2>", Wt::TextFormat::UnsafeXHTML);
	auto* type_row = form->addNew<Wt::WContainerWidget>();
	type_row->setStyleClass("kb-type-chips");
	m_type_id        = 0;
	const auto types = m_db.types_for_org(m_org_id);
	if(!is_new)
	{
		const bool valid = std::ranges::any_of(types, alt::equals{m_original.type_id}, &task_type_entry::id);
		if(valid)
		{
			m_type_id = m_original.type_id;
		}
	}

	auto add_chip = [&](long long chip_id, const std::string& lbl, const std::string& hex) {
		auto* chip = type_row->addNew<Wt::WContainerWidget>();
		chip->setStyleClass(chip_id == m_type_id ? "kb-type-chip selected" : "kb-type-chip");
		auto* dot = chip->addNew<Wt::WContainerWidget>();
		dot->setStyleClass("kb-type-chip__dot");
		if(hex.size() == 7 && hex[0] == '#')
		{
			try
			{
				dot->decorationStyle().setBackgroundColor(Wt::WColor{
				  std::stoi(hex.substr(1, 2), nullptr, 16),
				  std::stoi(hex.substr(3, 2), nullptr, 16),
				  std::stoi(hex.substr(5, 2), nullptr, 16)});
			}
			catch(...)
			{}
		}
		chip->addNew<Wt::WText>(lbl, Wt::TextFormat::Plain);
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
				const bool same_as_orig = (chip_id == m_original.type_id);
				if(same_as_orig)
				{
					unmark_field_dirty("type", nullptr);
				}
				else
				{
					mark_field_dirty("type", nullptr);
				}
			});
		}
	};
	add_chip(0, "(None)", "#cccccc");
	for(const auto& ty: types)
	{
		add_chip(ty.id, ty.name, ty.color);
	}
	if(!can_edit)
	{
		for(auto* c: m_type_chips)
		{
			c->setDisabled(true);
		}
	}

	// ── Comments (existing tasks only) ────────────────────────────────────────
	if(!is_new)
	{
		auto* cs = form->addNew<Wt::WContainerWidget>();
		cs->setStyleClass("kb-comment-section");
		cs->addNew<Wt::WText>("<h2>Comments</h2>", Wt::TextFormat::UnsafeXHTML);
		m_comment_list = cs->addNew<Wt::WContainerWidget>();
		m_comment_list->setStyleClass("kb-comment-list");
		if(caps.has_any(team_cap::comment) && !m_original.is_archived)
		{
			m_comment_compose = cs->addNew<Wt::WContainerWidget>();
			m_comment_compose->setStyleClass("kb-comment-compose");
		}
	}

	// ── Status message (validation / conflict) ───────────────────────────────
	m_status_msg = addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status_msg->setStyleClass("editor-status");
	m_status_msg->hide();

	// ── Button row ────────────────────────────────────────────────────────────
	auto* btn_row = addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	m_save_btn = btn_row->addNew<Wt::WPushButton>(is_new ? "Create Task" : "Save Changes");
	m_save_btn->setStyleClass("editor-btn");
	m_save_btn->clicked().connect([this] { save(); });
	if(!is_new)
	{
		m_save_btn->setEnabled(false);
	}
	if(!can_edit && !can_edit_status && !is_new)
	{
		m_save_btn->hide();
	}

	if(!is_new && caps.has_any(team_cap::archive_task))
	{
		if(m_original.is_archived)
		{
			auto* btn = btn_row->addNew<Wt::WPushButton>("Unarchive");
			btn->setStyleClass("editor-btn");
			btn->clicked().connect([this] {
				m_db.unarchive_task(m_original.id, m_username);
				live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
				live_hub::instance().broadcast("task:" + std::to_string(m_original.id));
				saved.emit();
			});
		}
		else
		{
			auto* btn = btn_row->addNew<Wt::WPushButton>("Archive");
			btn->setStyleClass("editor-btn editor-btn-danger");
			btn->clicked().connect([this, alive = m_alive] {
				if(!*alive)
				{
					return;
				}
				const long long tid = m_original.id;
				if(m_task_id != 0)
				{
					live_hub::instance().unsubscribe(
					  "task:" + std::to_string(m_task_id), m_session_id);
					live_hub::instance().unsubscribe(
					  "task:" + std::to_string(m_task_id) + ":comments", m_session_id);
					m_task_id = 0;
				}
				m_db.archive_task(tid, m_username);
				live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
				live_hub::instance().broadcast("task:" + std::to_string(tid));
				saved.emit();
			});
		}
	}

	{
		auto* cancel = btn_row->addNew<Wt::WPushButton>("Cancel");
		cancel->setStyleClass("editor-btn editor-btn-cancel");
		cancel->clicked().connect([this] { canceled.emit(); });
	}

	// ── Live-hub subscriptions (existing tasks) ───────────────────────────────
	if(!is_new)
	{
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
	}
}

// ── Destructor ────────────────────────────────────────────────────────────────

task_editor_form_widget::~task_editor_form_widget()
{
	*m_alive = false;
	if(m_task_id != 0)
	{
		live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
		live_hub::instance().unsubscribe(
		  "task:" + std::to_string(m_task_id) + ":comments", m_session_id);
	}
}

// ── Public accessors ──────────────────────────────────────────────────────────

bool task_editor_form_widget::is_dirty() const
{
	return !m_dirty_fields.empty();
}
bool task_editor_form_widget::is_stale() const
{
	return m_stale;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void task_editor_form_widget::mark_stale()
{
	m_stale = true;
	if(m_stale_banner)
	{
		m_stale_banner->show();
	}
	if(m_status_msg)
	{
		m_status_msg->setText("This task was modified by another user.");
		m_status_msg->show();
	}
	if(m_save_btn)
	{
		m_save_btn->setEnabled(false);
	}
}

void task_editor_form_widget::mark_field_dirty(
  const std::string&    field,
  Wt::WContainerWidget* container)
{
	m_dirty_fields.insert(field);
	if(container)
	{
		container->addStyleClass("kb-popup-field--dirty");
	}
	if(m_save_btn && !m_stale)
	{
		m_save_btn->setEnabled(true);
	}
}

void task_editor_form_widget::unmark_field_dirty(
  const std::string&    field,
  Wt::WContainerWidget* container)
{
	m_dirty_fields.erase(field);
	if(container)
	{
		container->removeStyleClass("kb-popup-field--dirty");
	}
	if(m_save_btn)
	{
		m_save_btn->setEnabled(!m_dirty_fields.empty() && !m_stale);
	}
}

void task_editor_form_widget::enter_edit_mode(Wt::WText* display, Wt::WWidget* edit)
{
	display->hide();
	edit->show();
	edit->setFocus(true);
}

void task_editor_form_widget::exit_edit_mode(
  Wt::WText*         display,
  Wt::WWidget*       edit,
  const std::string& new_text)
{
	edit->hide();
	display->setText(new_text);
	display->show();
}

void task_editor_form_widget::save()
{
	// Distinguish new vs edit
	const bool creating = (m_original.id == 0);

	if(!creating &&
	   (!m_caps.has_any(team_cap::edit_task_fields) &&
	    !m_caps.has_any(team_cap::edit_task_details)))
	{
		return;
	}
	if(!creating && m_original.is_archived)
	{
		return;
	}

	const std::string title = m_title_edit->text().toUTF8();
	if(title.empty())
	{
		if(m_status_msg)
		{
			m_status_msg->setText("Title is required.");
			m_status_msg->show();
		}
		return;
	}
	if(m_status_msg)
	{
		m_status_msg->hide();
	}

	const int         si = m_status_edit->currentIndex();
	const std::string status =
	  (si >= 0 && si < static_cast<int>(m_status_vals_used.size())) ? m_status_vals_used[si] : (creating ? "todo" : m_original.status);

	if(!creating && !m_caps.has_any(team_cap::edit_task_details))
	{
		if(status == "done" || m_original.status == "done")
		{
			if(m_status_msg)
			{
				m_status_msg->setText("Only leads can change status to or from Done.");
				m_status_msg->show();
			}
			return;
		}
	}

	kanban_task_entry t;
	t.team_id     = m_team_id;
	t.status      = status;
	t.title       = title;
	t.description = m_desc_editor->value();
	t.type_id     = m_type_id;
	if(const auto d = m_start_date_edit->date(); d.isValid())
	{
		t.start_date = d;
	}
	if(const auto d = m_end_date_edit->date(); d.isValid())
	{
		t.end_date = d;
	}

	if(!m_caps.has_any(team_cap::edit_task_details) && !creating)
	{
		t.title       = m_original.title;
		t.description = m_original.description;
		t.start_date  = m_original.start_date;
		t.end_date    = m_original.end_date;
		t.type_id     = m_original.type_id;
	}

	if(creating)
	{
		t.id = m_db.add_task(t, m_username);
		for(const auto& user: m_pending_assignees)
		{
			m_db.add_assignee(t.id, user, m_username);
		}
	}
	else
	{
		t.id         = m_original.id;
		t.sort_order = m_original.sort_order;
		try
		{
			m_db.update_task(t, m_username);
		}
		catch(const Wt::Dbo::StaleObjectException&)
		{
			mark_stale();
			return;
		}

		live_hub::instance().unsubscribe("task:" + std::to_string(m_task_id), m_session_id);
		live_hub::instance().unsubscribe(
		  "task:" + std::to_string(m_task_id) + ":comments", m_session_id);
		m_task_id = 0;
		live_hub::instance().broadcast("task:" + std::to_string(m_original.id));
	}

	live_hub::instance().broadcast("team:" + std::to_string(m_team_id));
	saved.emit();
}

void task_editor_form_widget::rebuild_history()
{
	if(!m_history_panel)
	{
		return;
	}
	m_history_panel->clear();

	if(m_task_id == 0 && m_original.id == 0)
	{
		m_history_panel->addNew<Wt::WText>("No history yet.", Wt::TextFormat::Plain);
		return;
	}
	const long long hid    = m_task_id != 0 ? m_task_id : m_original.id;
	const auto      events = m_db.history_for_task(hid);
	if(events.empty())
	{
		m_history_panel->addNew<Wt::WText>("No history yet.", Wt::TextFormat::Plain);
		return;
	}

	static const std::map<std::string, std::string> k_field_labels = {
	  {"status", "Status"}, {"title", "Title"}, {"description", "Description"}, {"assignees", "Assignees"}, {"start_date", "Start date"}, {"end_date", "End date"}, {"type", "Type"}};
	static const std::map<std::string, std::string> k_status_display = {
	  {"todo", "To Do"}, {"in_progress", "In Progress"}, {"review", "Review"}, {"done", "Done"}};

	auto display_val = [&](const std::string& field, const std::string& val) -> std::string {
		if(val.empty())
		{
			return "(unset)";
		}
		if(field == "status")
		{
			auto it = k_status_display.find(val);
			return it != k_status_display.end() ? it->second : val;
		}
		return val;
	};

	for(const auto& ev: events)
	{
		auto* entry = m_history_panel->addNew<Wt::WContainerWidget>();
		entry->setStyleClass("kb-history-entry");
		auto* hdr = entry->addNew<Wt::WText>(
		  fmt_ts(ev.occurred_at) + "  \xe2\x80\x94  " + ev.actor,
		  Wt::TextFormat::Plain);
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
				const auto lbl = k_field_labels.count(ch.field_name) ? k_field_labels.at(ch.field_name) : ch.field_name;
				entry->addNew<Wt::WText>(
				       lbl + ": " + display_val(ch.field_name, ch.old_value) +
				         " \xe2\x86\x92 " + display_val(ch.field_name, ch.new_value),
				       Wt::TextFormat::Plain)
				  ->setStyleClass("kb-history-line");
			}
		}
	}
}

void task_editor_form_widget::rebuild_comments()
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

	const long long cid = m_task_id != 0 ? m_task_id : m_original.id;
	if(cid == 0)
	{
		m_comment_list->addNew<Wt::WText>(
		  "Save the task first to add comments.", Wt::TextFormat::Plain);
		return;
	}

	const auto comments = m_db.comments_for_task(cid);
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
				  "Comment deleted by " + c.deleted_by + " \xe2\x80\x94 " +
				    fmt_ts(c.deleted_at),
				  Wt::TextFormat::Plain);
				continue;
			}
			auto* hdr2 = item->addNew<Wt::WContainerWidget>();
			hdr2->setStyleClass("kb-comment-header");
			hdr2->addNew<Wt::WText>(c.author, Wt::TextFormat::Plain)
			  ->setStyleClass("kb-comment-author");
			hdr2->addNew<Wt::WText>(
			  " \xe2\x80\x94 " + fmt_ts(c.created_at), Wt::TextFormat::Plain);
			auto* bw = item->addNew<markdown_viewer_widget>(c.body);
			bw->setStyleClass("kb-comment-body");
			if(!c.last_edited_at.empty())
			{
				item->addNew<Wt::WText>(
				      "Edited by " + c.last_edited_by + " at " + fmt_ts(c.last_edited_at),
				      Wt::TextFormat::Plain)
				  ->setStyleClass("kb-comment-meta");
			}

			const bool can_act = m_caps.has_any(team_cap::comment) &&
			                     (c.author == m_username ||
			                      m_caps.has_any(team_cap::manage_team));
			if(can_act)
			{
				auto* actions = item->addNew<Wt::WContainerWidget>();
				actions->setStyleClass("kb-comment-actions");
				auto* edit_btn = actions->addNew<Wt::WPushButton>("Edit");
				auto* del_btn  = actions->addNew<Wt::WPushButton>("Delete");
				del_btn->setStyleClass("kb-comment-del-btn");
				const long long   ccid    = c.id;
				const std::string cauthor = c.author;
				const bool        is_own  = (c.author == m_username);
				auto*             ea      = item->addNew<Wt::WContainerWidget>();
				ea->setStyleClass("kb-comment-edit-area");
				ea->hide();
				auto* eta   = ea->addNew<markdown_editor_widget>(c.body);
				auto* ebtns = ea->addNew<Wt::WContainerWidget>();
				ebtns->setStyleClass("kb-comment-edit-btns");
				auto* save_eb = ebtns->addNew<Wt::WPushButton>("Save");
				save_eb->setStyleClass("editor-btn");
				auto* cancel_eb = ebtns->addNew<Wt::WPushButton>("Cancel");
				cancel_eb->setStyleClass("editor-btn editor-btn-cancel");
				edit_btn->clicked().connect([this, cauthor, is_own, bw, ea, actions, eta] {
					if(is_own)
					{
						bw->hide();
						actions->hide();
						ea->show();
						eta->focus();
					}
					else
					{
						auto* d = new Wt::WDialog("Edit Another User's Comment");
						d->contents()->addNew<Wt::WText>(
						  "This comment was written by " + cauthor +
						    ". Are you sure you want to edit it?",
						  Wt::TextFormat::Plain);
						auto* yes = d->footer()->addNew<Wt::WPushButton>("Edit Anyway");
						yes->setStyleClass("editor-btn");
						auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
						no->setStyleClass("editor-btn editor-btn-cancel");
						yes->clicked().connect([d, bw, ea, actions, eta] {
							d->accept(); bw->hide(); actions->hide(); ea->show(); eta->focus(); });
						no->clicked().connect([d] { d->reject(); });
						d->finished().connect([d](Wt::DialogCode) { delete d; });
						d->show();
					}
				});
				cancel_eb->clicked().connect([bw, ea, actions] {
					ea->hide(); bw->show(); actions->show(); });
				save_eb->clicked().connect([this, ccid, eta, save_eb, alive = m_alive] {
					if(!*alive)
					{
						return;
					}
					const std::string nb = eta->value();
					if(nb.empty())
					{
						return;
					}
					save_eb->setDisabled(true);
					const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
					m_db.edit_comment(ccid, m_username, nb);
					live_hub::instance().broadcast("task:" + std::to_string(tid) + ":comments");
					rebuild_comments();
				});
				del_btn->clicked().connect([this, ccid, cauthor, is_own] {
					auto*             d   = new Wt::WDialog("Delete Comment");
					const std::string msg = is_own ? "Are you sure you want to delete this comment?" : "This comment was written by " + cauthor + ". Are you sure you want to delete it?";
					d->contents()->addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
					auto* yes = d->footer()->addNew<Wt::WPushButton>("Delete");
					yes->setStyleClass("editor-btn editor-btn-danger");
					auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
					no->setStyleClass("editor-btn editor-btn-cancel");
					yes->clicked().connect([this, d, ccid, alive = m_alive] {
						d->accept();
						if(!*alive)
						{
							return;
						}
						const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
						m_db.delete_comment(ccid, m_username);
						live_hub::instance().broadcast(
						  "task:" + std::to_string(tid) + ":comments");
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
	auto* ta   = m_comment_compose->addNew<markdown_editor_widget>();
	auto* post = m_comment_compose->addNew<Wt::WPushButton>("Post Comment");
	post->setStyleClass("editor-btn kb-comment-post-btn");
	post->setDisabled(true);
	ta->changed().connect([ta, post](const std::string& v) {
		post->setDisabled(v.empty());
	});
	post->clicked().connect([this, ta, post, alive = m_alive] {
		if(!*alive)
		{
			return;
		}
		const std::string body = ta->value();
		if(body.empty())
		{
			return;
		}
		post->setDisabled(true);
		const long long tid = m_task_id != 0 ? m_task_id : m_original.id;
		m_db.add_comment(tid, m_username, body);
		live_hub::instance().broadcast("task:" + std::to_string(tid) + ":comments");
		rebuild_comments();
	});
}