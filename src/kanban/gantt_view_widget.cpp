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

gantt_view_widget::gantt_view_widget(std::vector<kanban_task_entry>          tasks,
                                     const std::map<long long, std::string>& type_colors,
                                     std::function<void(long long)>          on_edit):
  m_type_colors{type_colors}, m_on_edit{std::move(on_edit)}
{
	Wt::WApplication::instance()->require("js/gantt.js?v=" BUILD_VERSION);
	setStyleClass("gv-wrap");

	auto* cb = addNew<Wt::WLineEdit>();
	cb->setStyleClass("kb-cb-hidden");
	m_cb_id = cb->id();
	cb->changed().connect([this, cb] {
		const std::string p = cb->text().toUTF8();
		if(p.rfind("EDIT:", 0) == 0 && m_on_edit)
		{
			long long tid = 0;
			try
			{
				tid = std::stoll(p.substr(5));
			}
			catch(...)
			{
				cb->setText(Wt::WString{});
				return;
			} // malformed payload
			m_on_edit(tid);
		}
		cb->setText(Wt::WString{});
	});

	auto* mount = addNew<Wt::WContainerWidget>();
	m_mount_id  = mount->id();

	doJavaScript("initGantt('" + m_mount_id + "'," + serialize_tasks(tasks) +
	             ",'" + m_cb_id + "');");
}

void gantt_view_widget::refresh(std::vector<kanban_task_entry>          tasks,
                                const std::map<long long, std::string>& type_colors)
{
	m_type_colors = type_colors;
	doJavaScript("initGantt('" + m_mount_id + "'," + serialize_tasks(tasks) +
	             ",'" + m_cb_id + "');");
}

std::string gantt_view_widget::serialize_tasks(const std::vector<kanban_task_entry>& tasks) const
{
	std::ostringstream ss;
	ss << '[';
	bool first = true;
	for(const auto& t: tasks)
	{
		if(t.status == "todo" || t.status == "done")
		{
			continue;
		}
		if(!first)
		{
			ss << ',';
		}
		first                   = false;
		const auto        it    = m_type_colors.find(t.type_id);
		const std::string color = (it != m_type_colors.end()) ? it->second : "#cccccc";
		ss << '{'
		   << "\"id\":" << t.id << ','
		   << "\"title\":\"" << escape_json_gv(t.title) << "\","
		   << "\"assigned_to\":\"" << escape_json_gv(t.assignees.empty() ? "" : t.assignees[0]) << "\","
		   << "\"color\":\"" << escape_json_gv(color) << "\","
		   << "\"start_date\":\"" << date_str_gv(t.start_date) << "\","
		   << "\"end_date\":\"" << date_str_gv(t.end_date) << "\""
		   << '}';
	}
	ss << ']';
	return ss.str();
}
