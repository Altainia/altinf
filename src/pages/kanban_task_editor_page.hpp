#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban.hpp"
#include "kanban/kanban_db.hpp"

#include <Wt/WColorPicker.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WDateEdit.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <functional>
#include <string>
#include <vector>

class kanban_task_editor_page: public Wt::WContainerWidget
{
public:
	kanban_task_editor_page(kanban_db&                        db,
	                        long long                         team_id,
	                        const session_data&               session,
	                        const kanban_task_entry*          existing,
	                        const std::vector<std::string>&   members,
	                        std::function<void()>             on_save);

private:
	kanban_db&               m_db;
	long long                m_team_id;
	const kanban_task_entry* m_existing;
	std::function<void()>    m_on_save;

	Wt::WLineEdit*    m_title{nullptr};
	Wt::WTextArea*    m_description{nullptr};
	Wt::WComboBox*    m_status{nullptr};
	Wt::WComboBox*    m_assigned_to{nullptr};
	Wt::WDateEdit*    m_start_date{nullptr};
	Wt::WDateEdit*    m_end_date{nullptr};
	Wt::WColorPicker* m_color{nullptr};
	Wt::WText*        m_status_msg{nullptr};

	static const std::vector<std::string> k_status_vals;
	static const std::vector<std::string> k_status_labels;

	void save();
};
