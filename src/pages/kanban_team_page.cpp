#include "kanban_team_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>

#include "auth/permission.hpp"

kanban_team_page::kanban_team_page(kanban_db& db, const session_data& session):
  m_db{db}
{
	setStyleClass("page kb-team-page");

	if(!session.permissions.has_any(permission::admin))
	{
		addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
		return;
	}

	const auto teams = db.all_teams();
	if(teams.empty())
	{
		addNew<Wt::WText>("No team configured.", Wt::TextFormat::Plain);
		return;
	}

	m_team_id = teams[0].id;

	addNew<Wt::WText>("<h1>Manage Team</h1>", Wt::TextFormat::UnsafeXHTML);

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("kb-team-form");

	// ── Team name ─────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Team Name</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* name_row = form->addNew<Wt::WContainerWidget>();
	name_row->setStyleClass("kb-team-name-row");

	auto* name_input = name_row->addNew<Wt::WLineEdit>();
	name_input->setStyleClass("editor-field");
	name_input->setText(teams[0].name);

	auto* rename_btn = name_row->addNew<Wt::WPushButton>("Save Name");
	rename_btn->setStyleClass("editor-btn");
	rename_btn->clicked().connect(
	  [this, name_input]() {
		  const std::string n = name_input->text().toUTF8();
		  if(!n.empty())
			  m_db.rename_team(m_team_id, n);
	  });

	// ── Members ───────────────────────────────────────────────────────────────
	form->addNew<Wt::WText>("<h2>Members</h2>", Wt::TextFormat::UnsafeXHTML);
	form->addNew<Wt::WText>(
	      "Members with the Board permission can create and edit tasks.",
	      Wt::TextFormat::Plain)
	  ->setStyleClass("kb-team-note");

	m_members_container = form->addNew<Wt::WContainerWidget>();
	m_members_container->setStyleClass("kb-members-container");

	for(const auto& u: db.members_for_team(m_team_id))
		add_member_row(u);

	auto* add_row = form->addNew<Wt::WContainerWidget>();
	add_row->setStyleClass("kb-member-add-row");

	m_member_input = add_row->addNew<Wt::WLineEdit>();
	m_member_input->setPlaceholderText("Username");
	m_member_input->setStyleClass("editor-field kb-member-input");

	auto* add_btn = add_row->addNew<Wt::WPushButton>("Add Member");
	add_btn->setStyleClass("editor-btn editor-btn-cancel");
	add_btn->clicked().connect(
	  [this]() {
		  const std::string u = m_member_input->text().toUTF8();
		  if(u.empty())
			  return;
		  for(const auto& r: m_member_rows)
			  if(r.username == u)
				  return;
		  m_db.add_member(m_team_id, u);
		  add_member_row(u);
		  m_member_input->setText("");
	  });

	// ── Footer ────────────────────────────────────────────────────────────────
	auto* back = form->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/board"}, "← Back to Board");
	back->setStyleClass("kb-back-link");
}

void kanban_team_page::add_member_row(const std::string& username)
{
	auto* row = m_members_container->addNew<Wt::WContainerWidget>();
	row->setStyleClass("kb-member-row");

	member_row r;
	r.container = row;
	r.username  = username;

	row->addNew<Wt::WText>(username, Wt::TextFormat::Plain)->setStyleClass("kb-member-name");

	auto* del_btn = row->addNew<Wt::WPushButton>("Remove");
	del_btn->setStyleClass("link-action-btn link-delete-btn");
	del_btn->clicked().connect(
	  [this, row, username]() {
		  m_db.remove_member(m_team_id, username);
		  remove_member_row(row);
	  });

	m_member_rows.push_back(std::move(r));
}

void kanban_team_page::remove_member_row(Wt::WContainerWidget* c)
{
	m_member_rows.erase(
	  std::remove_if(m_member_rows.begin(),
	                 m_member_rows.end(),
	                 [c](const member_row& r) { return r.container == c; }),
	  m_member_rows.end());
	m_members_container->removeWidget(c);
}
