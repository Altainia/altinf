#include "org_landing_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

org_landing_page::org_landing_page(org_db&             odb,
                                   kanban_db&          kdb,
                                   long long           org_id,
                                   const session_data& session,
                                   bool                is_org_lead)
{
	setStyleClass("page org-landing-page");

	const auto org = odb.find_org(org_id);
	if(!org)
	{
		addNew<Wt::WText>("Organisation not found.", Wt::TextFormat::Plain);
		return;
	}

	addNew<Wt::WText>("<h1>" + org->name + "</h1>", Wt::TextFormat::UnsafeXHTML);

	// ── Lead actions ──────────────────────────────────────────────────────────
	if(is_org_lead)
	{
		auto* actions = addNew<Wt::WContainerWidget>();
		actions->setStyleClass("org-lead-actions");

		actions->addNew<Wt::WAnchor>(
		         Wt::WLink{Wt::LinkType::InternalPath,
		                   "/org/" + std::to_string(org_id) + "/manage"},
		         "Manage organisation")
		  ->setStyleClass("editor-btn");

		actions->addNew<Wt::WAnchor>(
		         Wt::WLink{Wt::LinkType::InternalPath,
		                   "/org/" + std::to_string(org_id) + "/board"},
		         "View all teams\xe2\x80\x99 board")
		  ->setStyleClass("editor-btn editor-btn-cancel");
	}

	const auto  all_teams = kdb.teams_for_org(org_id);
	const auto& username  = session.username;

	// ── Your teams ────────────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Your teams</h2>", Wt::TextFormat::UnsafeXHTML);

	bool has_own = false;
	for(const auto& t: all_teams)
	{
		if(!kdb.is_member(t.id, username) && !is_org_lead)
		{
			continue;
		}
		has_own   = true;
		auto* row = addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-team-row");
		row->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath,
		               "/board/" + std::to_string(t.id)},
		     t.name)
		  ->setStyleClass("org-team-link");
	}
	if(!has_own)
	{
		addNew<Wt::WText>("You are not a member of any team in this organisation.",
		                  Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
	}

	// ── Other teams (for members who are not in those teams) ──────────────────
	bool has_other = false;
	for(const auto& t: all_teams)
	{
		if(kdb.is_member(t.id, username) || is_org_lead)
		{
			continue;
		}
		if(!has_other)
		{
			addNew<Wt::WText>("<h2>Other teams</h2>", Wt::TextFormat::UnsafeXHTML);
			has_other = true;
		}
		auto* row = addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-team-row org-team-row--other");
		row->addNew<Wt::WText>(t.name, Wt::TextFormat::Plain)
		  ->setStyleClass("org-team-name");
	}
}
