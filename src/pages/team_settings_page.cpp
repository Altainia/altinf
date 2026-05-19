#include "team_settings_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "widgets/live_hub.hpp"

team_settings_page::team_settings_page(kanban_db&          db,
                                       const session_data& session,
                                       long long           team_id)
{
	setStyleClass("page team-settings-page");

	const auto team = db.find_team(team_id);
	if(!team)
	{
		addNew<Wt::WText>("Team not found.", Wt::TextFormat::Plain);
		return;
	}

	const std::string board_url = "/board/" + std::to_string(team_id);
	const long long   org_id    = team->org_id;

	addNew<Wt::WText>("<h1>Team Settings \xe2\x80\x94 " + team->name + "</h1>",
	                  Wt::TextFormat::UnsafeXHTML);

	addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, board_url}, "\xe2\x86\x90 Back to board")
	  ->setStyleClass("editor-btn editor-btn-cancel");

	// ── Team name ──────────────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Team name</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* name_row = addNew<Wt::WContainerWidget>();
	name_row->setStyleClass("settings-field-row");

	auto* name_input = name_row->addNew<Wt::WLineEdit>();
	name_input->setText(team->name);
	name_input->setStyleClass("editor-field");

	auto* save_btn = name_row->addNew<Wt::WPushButton>("Save");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect(
	  [&db, team_id, org_id, name_input] {
		  const std::string n = name_input->text().toUTF8();
		  if(!n.empty())
		  {
			  db.rename_team(team_id, n);
			  live_hub::instance().broadcast("team:" + std::to_string(team_id));
			  live_hub::instance().broadcast("org:" + std::to_string(org_id));
		  }
	  });

	// ── Member permissions ─────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Member permissions</h2>", Wt::TextFormat::UnsafeXHTML);

	const auto settings = db.settings_for_team(team_id);

	auto* move_cb    = addNew<Wt::WCheckBox>("Members can move tasks between columns");
	auto* self_un_cb = addNew<Wt::WCheckBox>("Members can self-assign unassigned tasks");
	auto* self_as_cb =
	  addNew<Wt::WCheckBox>("Members can self-assign already-assigned tasks");
	auto* abandon_cb = addNew<Wt::WCheckBox>("Members can abandon tasks");

	move_cb->setChecked(settings.allow_member_move_columns);
	self_un_cb->setChecked(settings.allow_self_assign_unassigned);
	self_as_cb->setChecked(settings.allow_self_assign_assigned);
	abandon_cb->setChecked(settings.allow_abandon);

	for(auto* cb: {move_cb, self_un_cb, self_as_cb, abandon_cb})
	{
		cb->setStyleClass("settings-pref-check");
	}

	const std::string actor = session.username;

	move_cb->changed().connect(
	  [&db, team_id, actor, move_cb] {
		  auto s                      = db.settings_for_team(team_id);
		  s.allow_member_move_columns = move_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
	self_un_cb->changed().connect(
	  [&db, team_id, actor, self_un_cb] {
		  auto s                         = db.settings_for_team(team_id);
		  s.allow_self_assign_unassigned = self_un_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
	self_as_cb->changed().connect(
	  [&db, team_id, actor, self_as_cb] {
		  auto s                       = db.settings_for_team(team_id);
		  s.allow_self_assign_assigned = self_as_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
	abandon_cb->changed().connect(
	  [&db, team_id, actor, abandon_cb] {
		  auto s          = db.settings_for_team(team_id);
		  s.allow_abandon = abandon_cb->isChecked();
		  db.set_team_settings(s, actor);
		  live_hub::instance().broadcast("team:" + std::to_string(team_id));
	  });
}
