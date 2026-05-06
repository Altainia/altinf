#include "kanban_board_widget.hpp"

#include <Wt/WApplication.h>
#include <Wt/WLineEdit.h>

#include <sstream>

static std::string escape_json(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for(char c: s)
	{
		switch(c)
		{
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			out += c;
		}
	}
	return out;
}

static std::string date_str(const Wt::WDate& d)
{
	return d.isValid() ? d.toString("yyyy-MM-dd").toUTF8() : "";
}

kanban_board_widget::kanban_board_widget(
  std::vector<kanban_task_entry>                          tasks,
  bool                                                    can_edit,
  const std::map<long long, std::string>&                 type_colors,
  std::function<void(long long, const std::string&, int)> on_move,
  std::function<void(long long)>                          on_edit):
  m_type_colors{type_colors}
{
	Wt::WApplication::instance()->require("js/kanban.js?v=" BUILD_VERSION);

	setStyleClass("kb-board-wrap");

	m_mount    = addNew<Wt::WContainerWidget>();
	m_mount_id = m_mount->id();

	// Off-screen input: JS sets its value and fires 'change' to call back into C++.
	// setHidden(true) causes Wt to skip JS event binding; use CSS positioning instead.
	auto* cb = addNew<Wt::WLineEdit>();
	cb->setStyleClass("kb-cb-hidden");
	m_cb_id = cb->id();

	cb->changed().connect(
	  [cb, on_move = std::move(on_move), on_edit = std::move(on_edit)]() mutable {
		  const std::string payload = cb->text().toUTF8();
		  if(payload.starts_with("MOVE:"))
		  {
			  // MOVE:<task_id>:<new_status>:<sort_order>
			  const auto s1 = payload.find(':', 5);
			  const auto s2 = s1 != std::string::npos ? payload.find(':', s1 + 1) : std::string::npos;
			  if(s1 == std::string::npos || s2 == std::string::npos)
			  {
				  return;
			  }
			  const long long   tid    = std::stoll(payload.substr(5, s1 - 5));
			  const std::string status = payload.substr(s1 + 1, s2 - s1 - 1);
			  const int         sort   = std::stoi(payload.substr(s2 + 1));
			  on_move(tid, status, sort);
		  }
		  else if(payload.starts_with("EDIT:"))
		  {
			  const long long tid = std::stoll(payload.substr(5));
			  on_edit(tid);
		  }
	  });

	init_js(serialize_tasks(tasks), can_edit);
}

void kanban_board_widget::refresh(std::vector<kanban_task_entry>          tasks,
                                  bool                                    can_edit,
                                  const std::map<long long, std::string>& type_colors)
{
	m_type_colors = type_colors;
	init_js(serialize_tasks(tasks), can_edit);
}

std::string kanban_board_widget::serialize_tasks(const std::vector<kanban_task_entry>& tasks) const
{
	std::ostringstream ss;
	ss << '[';
	bool first = true;
	for(const auto& t: tasks)
	{
		if(!first)
		{
			ss << ',';
		}
		first                   = false;
		const auto        it    = m_type_colors.find(t.type_id);
		const std::string color = (it != m_type_colors.end()) ? it->second : "#cccccc";
		ss << '{'
		   << "\"id\":" << t.id << ','
		   << "\"status\":\"" << escape_json(t.status) << "\","
		   << "\"title\":\"" << escape_json(t.title) << "\","
		   << "\"assigned_to\":\"" << escape_json(t.assigned_to) << "\","
		   << "\"color\":\"" << escape_json(color) << "\","
		   << "\"start_date\":\"" << date_str(t.start_date) << "\","
		   << "\"end_date\":\"" << date_str(t.end_date) << "\""
		   << '}';
	}
	ss << ']';
	return ss.str();
}

void kanban_board_widget::init_js(const std::string& json, bool can_edit)
{
	doJavaScript("initKanban('" + m_mount_id + "'," + json + ",'" + m_cb_id + "'," + (can_edit ? "true" : "false") + ");");
}
