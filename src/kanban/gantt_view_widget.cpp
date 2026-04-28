#include "gantt_view_widget.hpp"

#include <Wt/WApplication.h>

#include <sstream>

static std::string escape_json_gv(const std::string& s)
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

static std::string date_str_gv(const Wt::WDate& d)
{
	return d.isValid() ? d.toString("yyyy-MM-dd").toUTF8() : "";
}

gantt_view_widget::gantt_view_widget(std::vector<kanban_task_entry> tasks)
{
	Wt::WApplication::instance()->require("js/gantt.js?v=" BUILD_VERSION);

	setStyleClass("gv-wrap");

	const std::string json     = serialize_tasks(tasks);
	const std::string mount_id = id();
	doJavaScript("initGantt('" + mount_id + "'," + json + ");");
}

std::string gantt_view_widget::serialize_tasks(const std::vector<kanban_task_entry>& tasks)
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
		first = false;
		ss << '{'
		   << "\"id\":" << t.id << ','
		   << "\"title\":\"" << escape_json_gv(t.title) << "\","
		   << "\"assigned_to\":\"" << escape_json_gv(t.assigned_to) << "\","
		   << "\"color\":\"" << escape_json_gv(t.color) << "\","
		   << "\"start_date\":\"" << date_str_gv(t.start_date) << "\","
		   << "\"end_date\":\"" << date_str_gv(t.end_date) << "\""
		   << '}';
	}
	ss << ']';
	return ss.str();
}
