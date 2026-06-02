#include "gantt_view_widget.hpp"

#include <Wt/WApplication.h>

#include <alt/functional.hpp>
#include <ranges>
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
                                     user_lookup::resolver                   resolve_user):
  m_type_colors{type_colors},
  m_resolve_user{std::move(resolve_user)}
{
	Wt::WApplication::instance()->require("js/gantt.js?v=" BUILD_VERSION);
	setStyleClass("gv-wrap");

	auto* cb = addNew<Wt::WLineEdit>();
	cb->setStyleClass("kb-cb-hidden");
	m_cb_id = cb->id();
	cb->changed().connect([this, cb] {
		const std::string p = cb->text().toUTF8();
		if(p.rfind("EDIT:", 0) == 0 || p.rfind("NAV:", 0) == 0)
		{
			const bool        is_nav = (p.rfind("NAV:", 0) == 0);
			const std::size_t prefix = is_nav ? 4 : 5;
			long long         tid    = 0;
			try
			{
				tid = std::stoll(p.substr(prefix));
			}
			catch(...)
			{
				cb->setText(Wt::WString{});
				return;
			} // malformed payload
			if(is_nav)
			{
				nav_requested.emit(tid);
			}
			else
			{
				edit_requested.emit(tid);
			}
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
	for(const auto& t: tasks | std::views::filter([](const auto& t) { return alt::none_of(alt::equals{t.status}, "todo", "done"); }))
	{
		if(!first)
		{
			ss << ',';
		}
		first                      = false;
		const auto        it       = m_type_colors.find(t.type_id);
		const std::string color    = (it != m_type_colors.end()) ? it->second : "#cccccc";
		const std::string assignee = t.assignees.empty() ? "" : t.assignees[0];
		const auto        info     = assignee.empty() ? user_lookup::info{} : m_resolve_user(assignee);
		ss << '{'
		   << "\"id\":" << t.id << ','
		   << "\"title\":\"" << escape_json_gv(t.title) << "\","
		   << "\"assigned_to\":\"" << escape_json_gv(assignee) << "\","
		   << "\"assigned_to_display\":\"" << escape_json_gv(info.display_name) << "\","
		   << "\"assigned_to_deleted\":" << (info.deleted ? "true" : "false") << ','
		   << "\"color\":\"" << escape_json_gv(color) << "\","
		   << "\"start_date\":\"" << date_str_gv(t.start_date) << "\","
		   << "\"end_date\":\"" << date_str_gv(t.end_date) << "\""
		   << '}';
	}
	ss << ']';
	return ss.str();
}