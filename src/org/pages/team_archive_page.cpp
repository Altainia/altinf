#include "team_archive_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "paths.hpp"

team_archive_page::team_archive_page(kanban_db&          db,
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

	addNew<Wt::WText>("<h1>Archived Tasks \xe2\x80\x94 " + team->name + "</h1>",
	                  Wt::TextFormat::UnsafeXHTML);

	addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, paths::team_kanban(team_id)},
	  "\xe2\x86\x90 Back to board")
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
		auto* row = list->addNew<Wt::WContainerWidget>();
		row->setStyleClass("kb-archive-row");

		row->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, paths::task_edit(task.id)},
		     task.title)
		  ->setStyleClass("kb-archive-title");

		row->addNew<Wt::WText>(task.status, Wt::TextFormat::Plain)
		  ->setStyleClass("kb-archive-status");
	}
}
