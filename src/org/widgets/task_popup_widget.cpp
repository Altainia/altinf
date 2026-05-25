#include "task_popup_widget.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLength.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "paths.hpp"
#include "task_editor_form_widget.hpp"

task_popup_widget::task_popup_widget(kanban_db& db, org_db& odb, long long task_id, const session_data& session, team_cap::flags caps, team_settings_entry settings, long long team_id): Wt::WDialog{}
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

	const std::string edit_url  = paths::task_edit(task_id);
	auto*             full_link = titleBar()->addNew<Wt::WAnchor>(
    Wt::WLink{Wt::LinkType::InternalPath, edit_url}, "Open full editor \xe2\x86\x97");
	full_link->setStyleClass("kb-popup-full-link");
	full_link->clicked().connect([this] { reject(); });

	m_form = contents()->addNew<task_editor_form_widget>(
	  db, odb, task_id, team_id, session, caps, settings);
	m_form->saved.connect([this] { accept(); });
	m_form->canceled.connect([this] { try_close(); });

	// Hidden input: JS → C++ close callback
	m_close_cb = contents()->addNew<Wt::WLineEdit>();
	m_close_cb->setStyleClass("kb-cb-hidden");
	const std::string cb_id = m_close_cb->id();
	m_close_cb->changed().connect([this] { try_close(); });

	// Constrain dialog height and make contents scroll.
	setMaximumSize(Wt::WLength::Auto, Wt::WLength(90, Wt::LengthUnit::ViewportHeight));
	contents()->setOverflow(Wt::Overflow::Auto, Wt::Orientation::Vertical);

	finished().connect([this, cb_id](Wt::DialogCode) {
		// Remove document listeners if popup closed without them firing
		// (e.g. Save button, Close button, archive).
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
	//
	// fireClose() does NOT remove the listeners — they stay registered so that
	// Escape / click-outside still work if the user dismisses the "Keep Editing"
	// confirm dialog and continues editing.  finished() does the final cleanup.
	//
	// Guards:
	//   - document.querySelectorAll('.Wt-dialog').length > 1  → a confirm dialog is
	//     already on screen; ignore the event to avoid stacking another one.
	//   - .Wt-datepicker                                      → WDateEdit calendar
	//     popup is appended to <body> outside .Wt-dialog; ignore clicks on it.
	doJavaScript(
	  "(function(cbId){"
	  "  var inp=document.getElementById(cbId);"
	  "  if(!inp)return;"
	  "  function fireClose(){"
	  "    inp.value='CLOSE';"
	  "    inp.dispatchEvent(new Event('change'));"
	  "  }"
	  "  function onEsc(e){"
	  "    if(e.key==='Escape'"
	  "       &&document.querySelectorAll('.Wt-dialog').length===1)"
	  "      fireClose();"
	  "  }"
	  "  function onDown(e){"
	  "    if(document.querySelectorAll('.Wt-dialog').length>1)return;"
	  "    if(e.target.closest('.Wt-dialog'))return;"
	  "    if(e.target.closest('.Wt-datepicker'))return;"
	  "    fireClose();"
	  "  }"
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