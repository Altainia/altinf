#pragma once

#include "auth/session_data.hpp"
#include "gantt/gantt.hpp"
#include "gantt/gantt_db.hpp"

#include <Wt/WColorPicker.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WDateEdit.h>
#include <Wt/WLineEdit.h>
#include <Wt/WTextArea.h>

#include <functional>
#include <string>
#include <vector>

class gantt_editor_page: public Wt::WContainerWidget
{
public:
	gantt_editor_page(gantt_db&                      db,
	                  const session_data&            session,
	                  const gantt_project_entry*     existing,
	                  std::vector<gantt_task_entry>  tasks,
	                  std::vector<std::string>       viewers,
	                  std::function<void(long long)> on_save);

private:
	struct task_row
	{
		Wt::WContainerWidget* container{nullptr};
		Wt::WLineEdit*        title{nullptr};
		Wt::WLineEdit*        assigned_to{nullptr};
		Wt::WDateEdit*        start_date{nullptr};
		Wt::WDateEdit*        end_date{nullptr};
		Wt::WColorPicker*     color{nullptr};
	};

	struct viewer_row
	{
		Wt::WContainerWidget* container{nullptr};
		std::string           username;
	};

	void add_task_row(const gantt_task_entry& task);
	void remove_task_row(Wt::WContainerWidget* c);
	void add_viewer_row(const std::string& username);
	void remove_viewer_row(Wt::WContainerWidget* c);
	void save();

	gantt_db&                      m_db;
	const session_data&            m_session;
	const gantt_project_entry*     m_existing;
	std::function<void(long long)> m_on_save;

	Wt::WLineEdit*        m_title{nullptr};
	Wt::WTextArea*        m_description{nullptr};
	Wt::WContainerWidget* m_tasks_container{nullptr};
	Wt::WContainerWidget* m_viewers_container{nullptr};
	Wt::WLineEdit*        m_viewer_input{nullptr};
	Wt::WText*            m_status{nullptr};

	std::vector<task_row>   m_task_rows;
	std::vector<viewer_row> m_viewer_rows;
};
