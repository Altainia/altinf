#include "kanban_task_editor_page.hpp"

#include <Wt/WApplication.h>
#include <Wt/WText.h>

#include "kanban/task_editor_form_widget.hpp"

kanban_task_editor_page::kanban_task_editor_page(
  kanban_db&               db,
  org_db&                  odb,
  long long                team_id,
  const session_data&      session,
  team_cap::flags          caps,
  const kanban_task_entry* existing,
  std::function<void()>    on_save)
{
	setStyleClass("page kb-editor-page");

	const bool is_new = (existing == nullptr);
	addNew<Wt::WText>(
	  is_new ? "<h1>New Task</h1>" : "<h1>Edit Task</h1>",
	  Wt::TextFormat::UnsafeXHTML);

	const long long   task_id   = existing ? existing->id : 0;
	const std::string board_url = "/board/" + std::to_string(team_id);

	addNew<task_editor_form_widget>(
	  db, odb, task_id, team_id, session, caps, team_settings_entry{},
	  on_save, // on_saved
	  [board_url] {
		  Wt::WApplication::instance()->setInternalPath(board_url, true);
	  });
}
