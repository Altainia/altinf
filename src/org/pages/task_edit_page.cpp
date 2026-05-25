#include "task_edit_page.hpp"

#include <Wt/WApplication.h>
#include <Wt/WText.h>

#include "org/widgets/task_editor_form_widget.hpp"
#include "paths.hpp"

task_edit_page::task_edit_page(
  kanban_db&               db,
  org_db&                  odb,
  long long                team_id,
  const session_data&      session,
  team_cap::flags          caps,
  team_settings_entry      settings,
  const kanban_task_entry* existing)
{
	setStyleClass("page kb-editor-page");

	const bool is_new = (existing == nullptr);
	addNew<Wt::WText>(
	  is_new ? "<h1>New Task</h1>" : "<h1>Edit Task</h1>",
	  Wt::TextFormat::UnsafeXHTML);

	const long long task_id = existing ? existing->id : 0;

	auto* form = addNew<task_editor_form_widget>(
	  db, odb, task_id, team_id, session, caps, settings);

	form->saved.connect([this] { saved.emit(); });
	form->canceled.connect([team_id] {
		Wt::WApplication::instance()->setInternalPath(paths::team_kanban(team_id), true);
	});
}
