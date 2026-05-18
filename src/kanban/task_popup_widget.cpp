#include "task_popup_widget.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "task_editor_form_widget.hpp"

task_popup_widget::task_popup_widget(kanban_db& db, org_db& odb, long long task_id, const session_data& session, team_cap::flags caps, long long team_id): Wt::WDialog{}
{
	addStyleClass("kb-task-popup");

	const auto task_opt = db.find_task(task_id);
	if(!task_opt)
	{
		setWindowTitle("Task not found");
		contents()->addNew<Wt::WText>("Task not found.", Wt::TextFormat::Plain);
		auto* close_btn = footer()->addNew<Wt::WPushButton>("Close");
		close_btn->setStyleClass("editor-btn editor-btn-cancel");
		close_btn->clicked().connect([this] { reject(); });
		finished().connect([this](Wt::DialogCode) { delete this; });
		show();
		return;
	}

	setWindowTitle(task_opt->title);

	const std::string edit_url = "/board/" + std::to_string(team_id) +
	                             "/task/" + std::to_string(task_id) + "/edit";
	auto* full_link = titleBar()->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, edit_url}, "Open full editor \xe2\x86\x97");
	full_link->setStyleClass("kb-popup-full-link");
	full_link->clicked().connect([this] { reject(); });

	m_form = contents()->addNew<task_editor_form_widget>(
	  db, odb, task_id, team_id, session, caps, [this] { accept(); }, [this] { try_close(); });

	// Hidden input: JS → C++ close callback
	m_close_cb = contents()->addNew<Wt::WLineEdit>();
	m_close_cb->setStyleClass("kb-cb-hidden");
	const std::string cb_id = m_close_cb->id();
	m_close_cb->changed().connect([this] { try_close(); });

	auto* close_btn = footer()->addNew<Wt::WPushButton>("Close");
	close_btn->setStyleClass("editor-btn editor-btn-cancel");
	close_btn->clicked().connect([this] { try_close(); });

	finished().connect([this, cb_id](Wt::DialogCode) {
		// Clean up document listeners if popup was closed without firing them
		// (e.g. Save button, Cancel button).
		doJavaScript(
		  "(function(){"
		  "  var inp=document.getElementById('" +
		  cb_id +
		  "');"
		  "  if(!inp)return;"
		  "  if(inp.__onEsc)document.removeEventListener('keydown',inp.__onEsc);"
		  "  if(inp.__onDown)document.removeEventListener('mousedown',inp.__onDown);"
		  "})();");
		delete this;
	});
	show();

	// Wire Escape key and click-outside to try_close().
	// Listeners are stored on inp.__onEsc / inp.__onDown so finished() can clean up
	// on non-Escape / non-click close paths (Save, Cancel button).
	doJavaScript(
	  "(function(cbId){"
	  "  var inp=document.getElementById(cbId);"
	  "  if(!inp)return;"
	  "  function fireClose(){"
	  "    document.removeEventListener('keydown',inp.__onEsc);"
	  "    document.removeEventListener('mousedown',inp.__onDown);"
	  "    inp.value='CLOSE';"
	  "    inp.dispatchEvent(new Event('change'));"
	  "  }"
	  "  function onEsc(e){ if(e.key==='Escape')fireClose(); }"
	  "  function onDown(e){ if(!e.target.closest('.Wt-dialog'))fireClose(); }"
	  "  inp.__onEsc=onEsc;"
	  "  inp.__onDown=onDown;"
	  "  document.addEventListener('keydown',onEsc);"
	  "  document.addEventListener('mousedown',onDown);"
	  "})('" +
	  cb_id + "');");
}

task_popup_widget::~task_popup_widget() = default;

void task_popup_widget::try_close()
{
	if(m_form && m_form->is_dirty())
	{
		auto* d = new Wt::WDialog("Unsaved Changes");
		d->contents()->addNew<Wt::WText>(
		  "You have unsaved changes. Discard them?", Wt::TextFormat::Plain);
		auto* discard = d->footer()->addNew<Wt::WPushButton>("Discard Changes");
		discard->setStyleClass("editor-btn editor-btn-danger");
		auto* keep = d->footer()->addNew<Wt::WPushButton>("Keep Editing");
		keep->setStyleClass("editor-btn editor-btn-cancel");
		discard->clicked().connect([this, d] { d->accept(); reject(); });
		keep->clicked().connect([d] { d->reject(); });
		d->finished().connect([d](Wt::DialogCode) { delete d; });
		d->show();
	}
	else
	{
		reject();
	}
}
