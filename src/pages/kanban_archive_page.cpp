#include "kanban_archive_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

kanban_archive_page::kanban_archive_page(kanban_db&          db,
                                         const session_data& session,
                                         long long           team_id)
{
	(void)session;

	setStyleClass("page kb-archive-page");

	const auto team = db.find_team(team_id);
	if(!team)
	{
		addNew<Wt::WText>("Team not found.", Wt::TextFormat::Plain);
		return;
	}

	const std::string board_url = "/board/" + std::to_string(team_id);

	addNew<Wt::WText>("<h1>Archived Tasks \xe2\x80\x94 " + team->name + "</h1>",
	                  Wt::TextFormat::UnsafeXHTML);

	addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, board_url}, "\xe2\x86\x90 Back to board")
	  ->setStyleClass("editor-btn editor-btn-cancel");

	const auto tasks = db.archived_tasks_for_team(team_id);
	if(tasks.empty())
	{
		addNew<Wt::WText>("No archived tasks.", Wt::TextFormat::Plain)
		  ->setStyleClass("kb-archive-empty");
		return;
	}

	auto* list = addNew<Wt::WContainerWidget>();
	list->setStyleClass("kb-archive-list");

	for(const auto& task: tasks)
	{
		const std::string edit_url =
		  board_url + "/task/" + std::to_string(task.id) + "/edit";

		auto* row = list->addNew<Wt::WContainerWidget>();
		row->setStyleClass("kb-archive-row");

		row->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, edit_url},
		     task.title)
		  ->setStyleClass("kb-archive-title");

		row->addNew<Wt::WText>(task.status, Wt::TextFormat::Plain)
		  ->setStyleClass("kb-archive-status");
	}
}
