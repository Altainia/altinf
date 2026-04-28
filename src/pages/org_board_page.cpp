#include "org_board_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include "kanban/kanban_board_widget.hpp"

org_board_page::org_board_page(org_db&             odb,
                               kanban_db&          kdb,
                               long long           org_id,
                               const session_data& session)
{
	setStyleClass("page org-board-page");

	const auto org = odb.find_org(org_id);
	if(!org)
	{
		addNew<Wt::WText>("Organisation not found.", Wt::TextFormat::Plain);
		return;
	}

	// ── Header ────────────────────────────────────────────────────────────────
	auto* hdr = addNew<Wt::WContainerWidget>();
	hdr->setStyleClass("kb-page-hdr");
	hdr->addNew<Wt::WText>("<h1>" + org->name + " \xe2\x80\x94 All Teams</h1>",
	                       Wt::TextFormat::UnsafeXHTML);
	hdr->addNew<Wt::WAnchor>(
	     Wt::WLink{Wt::LinkType::InternalPath, "/org/" + std::to_string(org_id)},
	     "\xe2\x86\x90 Back to organisation")
	  ->setStyleClass("kb-back-link");

	const auto teams = kdb.teams_for_org(org_id);
	if(teams.empty())
	{
		addNew<Wt::WText>("No teams in this organisation yet.", Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
		return;
	}

	// ── One board per team ────────────────────────────────────────────────────
	for(const auto& team: teams)
	{
		auto* section = addNew<Wt::WContainerWidget>();
		section->setStyleClass("org-board-section");

		auto* team_hdr = section->addNew<Wt::WContainerWidget>();
		team_hdr->setStyleClass("org-board-team-hdr");
		team_hdr->addNew<Wt::WText>(
		  "<h2>" + team.name + "</h2>", Wt::TextFormat::UnsafeXHTML);
		team_hdr->addNew<Wt::WAnchor>(
		          Wt::WLink{Wt::LinkType::InternalPath,
		                    "/board/" + std::to_string(team.id)},
		          "Open full board")
		  ->setStyleClass("kb-back-link");

		const auto      tasks = kdb.tasks_for_team(team.id);
		const long long tid   = team.id;

		// Org leads can edit all teams; capture tid by value for the lambdas.
		auto* board = section->addNew<kanban_board_widget>(
		  tasks,
		  true, // is_lead — org leads always have edit rights
		  [&kdb, tid](long long task_id, const std::string& status, int sort) {
			  kdb.update_task_status(task_id, status, sort);
		  },
		  [tid](long long task_id) {
			  Wt::WApplication::instance()->setInternalPath(
			    "/board/" + std::to_string(tid) + "/task/" +
			      std::to_string(task_id) + "/edit",
			    true);
		  });
		(void)board;
	}
}
