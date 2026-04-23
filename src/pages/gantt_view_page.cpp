#include "gantt_view_page.h"

#include "auth/permission.h"

#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>

int gantt_view_page::date_to_days(const std::string& d)
{
	int y{1970}, m{1}, day{1};
	std::sscanf(d.c_str(), "%d-%d-%d", &y, &m, &day);
	std::tm tm{};
	tm.tm_year  = y - 1900;
	tm.tm_mon   = m - 1;
	tm.tm_mday  = day;
	tm.tm_isdst = -1;
	return static_cast<int>(std::mktime(&tm) / 86400);
}

std::string gantt_view_page::days_to_date_label(int days)
{
	const std::time_t t  = static_cast<std::time_t>(days) * 86400;
	std::tm*          tm = std::gmtime(&t);
	char              buf[12];
	std::strftime(buf, sizeof(buf), "%b %d", tm);
	return buf;
}

gantt_view_page::gantt_view_page(const gantt_project_entry&           project,
                                 const std::vector<gantt_task_entry>& tasks,
                                 const session_data&                  session,
                                 std::function<void()>                on_edit)
{
	setStyleClass("page gantt-view-page");

	// Header
	auto* hdr = addNew<Wt::WContainerWidget>();
	hdr->setStyleClass("gantt-view-hdr");

	auto* title = hdr->addNew<Wt::WText>(project.title, Wt::TextFormat::Plain);
	title->setStyleClass("gantt-view-title");

	const bool user_can_edit =
	  has_permission(session.permissions, permission::admin) ||
	  (has_permission(session.permissions, permission::gantt_write) &&
	   project.owner_username == session.username);
	if(user_can_edit)
	{
		auto* edit_btn = hdr->addNew<Wt::WPushButton>("Edit Chart");
		edit_btn->setStyleClass("editor-btn gantt-edit-btn");
		edit_btn->clicked().connect([on_edit = std::move(on_edit)]() mutable { on_edit(); });
	}

	if(!project.description.empty())
	{
		auto* desc = addNew<Wt::WText>(project.description, Wt::TextFormat::Plain);
		desc->setStyleClass("gantt-view-desc");
	}

	m_chart_wrap = addNew<Wt::WContainerWidget>();
	m_chart_wrap->setStyleClass("gantt-chart-wrap");

	render_chart(tasks);
}

void gantt_view_page::render_chart(const std::vector<gantt_task_entry>& tasks)
{
	if(tasks.empty())
	{
		m_chart_wrap->addNew<Wt::WText>("No tasks yet.", Wt::TextFormat::Plain)
		  ->setStyleClass("gantt-empty");
		return;
	}

	// Compute date range
	int min_day = date_to_days(tasks.front().start_date);
	int max_day = date_to_days(tasks.front().end_date);
	for(const auto& t: tasks)
	{
		min_day = std::min(min_day, date_to_days(t.start_date));
		max_day = std::max(max_day, date_to_days(t.end_date));
	}
	const int total_days = max_day - min_day + 1;
	if(total_days <= 0)
		return;

	auto* rows = m_chart_wrap->addNew<Wt::WContainerWidget>();
	rows->setStyleClass("gantt-rows");

	// Week-marker header row
	{
		auto* hdr_row = rows->addNew<Wt::WContainerWidget>();
		hdr_row->setStyleClass("gantt-row gantt-header-row");

		hdr_row->addNew<Wt::WContainerWidget>()->setStyleClass("gantt-label");

		auto* track = hdr_row->addNew<Wt::WContainerWidget>();
		track->setStyleClass("gantt-track");

		// First week starts on min_day rounded down to Monday, or just min_day
		for(int d = 0; d < total_days; d += 7)
		{
			const int   abs_day  = min_day + d;
			const float left_pct = static_cast<float>(d) / static_cast<float>(total_days) * 100.0f;

			auto* marker = track->addNew<Wt::WText>(
			  days_to_date_label(abs_day), Wt::TextFormat::Plain);
			marker->setStyleClass("gantt-week-marker");
			marker->setAttributeValue(
			  "style", "left:" + std::to_string(static_cast<int>(left_pct)) + "%");
		}
	}

	// Sort a stable copy by assignee (unassigned last), then preserve sort_order within group
	auto sorted = tasks;
	std::stable_sort(sorted.begin(), sorted.end(), [](const gantt_task_entry& a, const gantt_task_entry& b) {
		const bool ae = a.assigned_to.empty();
		const bool be = b.assigned_to.empty();
		if(ae != be)
			return be; // non-empty before empty
		return a.assigned_to < b.assigned_to;
	});

	// Task rows grouped by assignee
	std::string current_assignee{"\x01"};
	for(const auto& task: sorted)
	{
		if(task.assigned_to != current_assignee)
		{
			current_assignee          = task.assigned_to;
			const std::string hdr_txt = current_assignee.empty() ? "Unassigned" : current_assignee;

			auto* grp_hdr = rows->addNew<Wt::WContainerWidget>();
			grp_hdr->setStyleClass("gantt-row gantt-group-hdr");
			grp_hdr->addNew<Wt::WText>(hdr_txt, Wt::TextFormat::Plain);
		}

		const int   start_off = date_to_days(task.start_date) - min_day;
		const int   end_off   = date_to_days(task.end_date) - min_day;
		const float left_pct  = static_cast<float>(std::max(0, start_off)) /
		                       static_cast<float>(total_days) * 100.0f;
		const float width_pct = static_cast<float>(std::max(1, end_off - start_off + 1)) /
		                        static_cast<float>(total_days) * 100.0f;

		auto* row = rows->addNew<Wt::WContainerWidget>();
		row->setStyleClass("gantt-row");

		auto* label = row->addNew<Wt::WContainerWidget>();
		label->setStyleClass("gantt-label gantt-task-label");
		label->addNew<Wt::WText>(task.title, Wt::TextFormat::Plain);

		auto* track = row->addNew<Wt::WContainerWidget>();
		track->setStyleClass("gantt-track");

		auto* bar = track->addNew<Wt::WContainerWidget>();
		bar->setStyleClass("gantt-bar");

		const std::string color = task.color.empty() ? "#7aa2d4" : task.color;
		bar->setAttributeValue(
		  "style",
		  "left:" + std::to_string(static_cast<int>(left_pct)) + "%;" +
		    "width:" + std::to_string(static_cast<int>(std::max(1.0f, width_pct))) + "%;" +
		    "background:" + color);

		auto* bar_label = bar->addNew<Wt::WText>(
		  task.start_date + " \xe2\x80\x93 " + task.end_date, Wt::TextFormat::Plain);
		bar_label->setStyleClass("gantt-bar-dates");
	}
}
