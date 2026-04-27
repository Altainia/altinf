#include "gantt_view_page.hpp"

#include <Wt/WDate.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>
#include <string>

#include "auth/permission.hpp"

std::string gantt_view_page::date_label(const Wt::WDate& d)
{
	return d.toString("MMM dd").toUTF8();
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
	  session.permissions.has_any(permission::admin) ||
	  (session.permissions.has_any(permission::gantt_write) &&
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
	const Wt::WDate today = Wt::WDate::currentDate();

	// Keep only tasks with a valid end date that is today or in the future
	std::vector<gantt_task_entry> active;
	for(const auto& t: tasks)
	{
		if(t.end_date.isValid() && t.end_date >= today)
		{
			active.push_back(t);
		}
	}

	if(active.empty())
	{
		m_chart_wrap->addNew<Wt::WText>(
		              tasks.empty() ? "No tasks yet." : "No active tasks.", Wt::TextFormat::Plain)
		  ->setStyleClass("gantt-empty");
		return;
	}

	// Compute date range across active tasks
	Wt::WDate min_date = active.front().start_date.isValid() ? active.front().start_date : active.front().end_date;
	Wt::WDate max_date = active.front().end_date;
	for(const auto& t: active)
	{
		if(t.start_date.isValid() && t.start_date < min_date)
		{
			min_date = t.start_date;
		}
		if(t.end_date.isValid() && t.end_date > max_date)
		{
			max_date = t.end_date;
		}
	}

	const int total_days = min_date.daysTo(max_date) + 1;
	if(total_days <= 0)
	{
		return;
	}

	auto* rows = m_chart_wrap->addNew<Wt::WContainerWidget>();
	rows->setStyleClass("gantt-rows");

	// Week-marker header row
	{
		auto* hdr_row = rows->addNew<Wt::WContainerWidget>();
		hdr_row->setStyleClass("gantt-row gantt-header-row");

		hdr_row->addNew<Wt::WContainerWidget>()->setStyleClass("gantt-label");

		auto* track = hdr_row->addNew<Wt::WContainerWidget>();
		track->setStyleClass("gantt-track");

		for(int d = 0; d < total_days; d += 7)
		{
			const float left_pct = static_cast<float>(d) / static_cast<float>(total_days) * 100.0f;

			auto* marker = track->addNew<Wt::WText>(
			  date_label(min_date.addDays(d)), Wt::TextFormat::Plain);
			marker->setStyleClass("gantt-week-marker");
			marker->setAttributeValue(
			  "style", "left:" + std::to_string(static_cast<int>(left_pct)) + "%");
		}
	}

	// Sort a stable copy by assignee (unassigned last), then preserve sort_order within group
	auto sorted = active;
	std::stable_sort(sorted.begin(), sorted.end(), [](const gantt_task_entry& a, const gantt_task_entry& b) {
		const bool ae = a.assigned_to.empty();
		const bool be = b.assigned_to.empty();
		if(ae != be)
		{
			return be; // non-empty before empty
		}
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

		const int   start_off = task.start_date.isValid() ? min_date.daysTo(task.start_date) : 0;
		const int   end_off   = task.end_date.isValid() ? min_date.daysTo(task.end_date) : 0;
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

		const std::string start_str =
		  task.start_date.isValid() ? task.start_date.toString("yyyy-MM-dd").toUTF8() : "";
		const std::string end_str =
		  task.end_date.isValid() ? task.end_date.toString("yyyy-MM-dd").toUTF8() : "";
		auto* bar_label = bar->addNew<Wt::WText>(
		  start_str + " \xe2\x80\x93 " + end_str, Wt::TextFormat::Plain);
		bar_label->setStyleClass("gantt-bar-dates");
	}
}
