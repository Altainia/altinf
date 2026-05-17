#include "task_editor_form_widget.hpp"

const std::vector<std::string> task_editor_form_widget::k_status_vals   = {"todo", "in_progress", "review", "done"};
const std::vector<std::string> task_editor_form_widget::k_status_labels = {"To Do", "In Progress", "Review", "Done"};

task_editor_form_widget::task_editor_form_widget(
  kanban_db&            db,
  org_db&               odb,
  long long             task_id,
  long long             team_id,
  const session_data&   session,
  team_cap::flags       caps,
  std::function<void()> on_saved,
  std::function<void()> on_cancel): m_db{db},
                                    m_odb{odb},
                                    m_task_id{task_id},
                                    m_team_id{team_id},
                                    m_username{session.username},
                                    m_caps{caps},
                                    m_on_saved{std::move(on_saved)},
                                    m_on_cancel{std::move(on_cancel)}
{}

task_editor_form_widget::~task_editor_form_widget()
{
	*m_alive = false;
}
bool task_editor_form_widget::is_dirty() const
{
	return !m_dirty_fields.empty();
}
bool task_editor_form_widget::is_stale() const
{
	return m_stale;
}
void task_editor_form_widget::save()
{}
void task_editor_form_widget::mark_stale()
{}
void task_editor_form_widget::mark_field_dirty(const std::string&, Wt::WContainerWidget*)
{}
void task_editor_form_widget::unmark_field_dirty(const std::string&, Wt::WContainerWidget*)
{}
void task_editor_form_widget::enter_edit_mode(Wt::WText*, Wt::WWidget*)
{}
void task_editor_form_widget::exit_edit_mode(Wt::WText*, Wt::WWidget*, const std::string&)
{}
void task_editor_form_widget::rebuild_comments()
{}
void task_editor_form_widget::rebuild_history()
{}
std::string task_editor_form_widget::render_markdown(const std::string& md) const
{
	return md;
}
