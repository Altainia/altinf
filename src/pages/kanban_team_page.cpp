#include "kanban_team_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "org/org.hpp"
#include "widgets/live_hub.hpp"

kanban_team_page::kanban_team_page(org_db&             odb,
                                   kanban_db&          kdb,
                                   user_db&            udb,
                                   long long           org_id,
                                   const session_data& session,
                                   const std::string&  back_url):
  m_odb{odb},
  m_kdb{kdb},
  m_udb{udb},
  m_org_id{org_id},
  m_session{session}
{
	setStyleClass("page kb-team-page");

	const auto org = odb.find_org(org_id);
	if(!org)
	{
		addNew<Wt::WText>("Organisation not found.", Wt::TextFormat::Plain);
		return;
	}

	m_org_name = org->name;

	addNew<Wt::WText>("<h1>Manage: " + org->name + "</h1>",
	                  Wt::TextFormat::UnsafeXHTML);

	const std::string effective_back = back_url.empty() ? "/org/" + std::to_string(org_id) : back_url;
	addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, effective_back},
	  "\xe2\x86\x90 Back")
	  ->setStyleClass("kb-back-link");

	// ── Members ───────────────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Members</h2>", Wt::TextFormat::UnsafeXHTML);
	m_members_section = addNew<Wt::WContainerWidget>();
	m_members_section->setStyleClass("kb-members-container");
	refresh_members();

	// ── Pending invites ───────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Pending invites</h2>", Wt::TextFormat::UnsafeXHTML);
	m_pending_section = addNew<Wt::WContainerWidget>();
	m_pending_section->setStyleClass("kb-members-container");
	refresh_pending();

	// ── Invite form ───────────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Invite user</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* invite_row = addNew<Wt::WContainerWidget>();
	invite_row->setStyleClass("kb-member-add-row");

	m_invite_input = invite_row->addNew<Wt::WLineEdit>();
	m_invite_input->setPlaceholderText("Username");
	m_invite_input->setStyleClass("editor-field kb-member-input");

	m_invite_lead = invite_row->addNew<Wt::WCheckBox>("Invite as lead");
	m_invite_lead->setStyleClass("kb-lead-check");

	auto* invite_btn = invite_row->addNew<Wt::WPushButton>("Send invite");
	invite_btn->setStyleClass("editor-btn");
	invite_btn->clicked().connect(
	  [this] {
		  const std::string u = m_invite_input->text().toUTF8();
		  if(u.empty())
		  {
			  m_invite_msg->setText("Enter a username.");
			  return;
		  }
		  if(!m_udb.username_exists(u))
		  {
			  m_invite_msg->setText("User \"" + u + "\" does not exist.");
			  return;
		  }
		  m_odb.invite_to_org(m_org_id, u, m_invite_lead->isChecked());
		  live_hub::instance().broadcast("user:" + u);
		  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
		  m_invite_input->setText("");
		  m_invite_lead->setChecked(false);
		  m_invite_msg->setText("Invite sent to " + u + ".");
		  refresh_pending();
	  });

	m_invite_msg = addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_invite_msg->setStyleClass("editor-status");

	// ── Teams ─────────────────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Teams</h2>", Wt::TextFormat::UnsafeXHTML);
	m_teams_section = addNew<Wt::WContainerWidget>();
	m_teams_section->setStyleClass("kb-teams-container");
	refresh_teams();

	// ── New team form ─────────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Create team</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* new_team_row = addNew<Wt::WContainerWidget>();
	new_team_row->setStyleClass("kb-member-add-row");

	m_new_team_input = new_team_row->addNew<Wt::WLineEdit>();
	m_new_team_input->setPlaceholderText("Team name");
	m_new_team_input->setStyleClass("editor-field");

	auto* new_team_btn = new_team_row->addNew<Wt::WPushButton>("Create");
	new_team_btn->setStyleClass("editor-btn");
	new_team_btn->clicked().connect(
	  [this] {
		  const std::string name = m_new_team_input->text().toUTF8();
		  if(name.empty())
		  {
			  return;
		  }
		  m_kdb.create_team(name, m_org_id);
		  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
		  m_new_team_input->setText("");
		  refresh_teams();
	  });

	m_session_id = Wt::WApplication::instance()->sessionId();
	live_hub::instance().subscribe(
	  "org:" + std::to_string(m_org_id),
	  m_session_id,
	  [this, alive = m_alive] {
		  if(*alive)
		  {
			  refresh();
			  Wt::WApplication::instance()->triggerUpdate();
		  }
	  });
}

kanban_team_page::~kanban_team_page()
{
	*m_alive = false;
	if(!m_session_id.empty())
	{
		live_hub::instance().unsubscribe(
		  "org:" + std::to_string(m_org_id), m_session_id);
	}
}

void kanban_team_page::refresh()
{
	refresh_members();
	refresh_pending();
	refresh_teams();
}

void kanban_team_page::refresh_members()
{
	m_members_section->clear();
	const auto members = m_odb.org_members(m_org_id);
	if(members.empty())
	{
		m_members_section->addNew<Wt::WText>("No active members.",
		                                     Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
		return;
	}
	for(const auto& m: members)
	{
		auto* row = m_members_section->addNew<Wt::WContainerWidget>();
		row->setStyleClass("kb-member-row");

		row->addNew<Wt::WText>(m.username + (m.is_lead ? " (lead)" : ""),
		                       Wt::TextFormat::Plain)
		  ->setStyleClass("kb-member-name");

		// Lead toggle — blocked server-side if it would leave 0 leads.
		const std::string toggle_label = m.is_lead ? "Demote" : "Make lead";
		auto*             toggle       = row->addNew<Wt::WPushButton>(toggle_label);
		toggle->setStyleClass("link-action-btn");
		toggle->clicked().connect(
		  [this, uid = m.username, currently_lead = m.is_lead] {
			  if(!m_odb.set_org_lead(m_org_id, uid, !currently_lead))
			  {
				  m_invite_msg->setText(
				    "Cannot demote: organisation must have at least one lead.");
			  }
			  else
			  {
				  const std::string type =
				    currently_lead ? "org_lead_demoted" : "org_lead_promoted";
				  m_odb.push_notification(
				    uid, type, make_org_event_payload(m_org_id, m_org_name));
				  live_hub::instance().broadcast("user:" + uid);
				  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
				  m_invite_msg->setText("");
			  }
			  refresh_members();
		  });

		// Remove — blocked server-side if last lead.
		auto* del = row->addNew<Wt::WPushButton>("Remove");
		del->setStyleClass("link-action-btn link-delete-btn");
		del->clicked().connect(
		  [this, uid = m.username] {
			  if(!m_odb.remove_org_member(m_org_id, uid))
			  {
				  m_invite_msg->setText(
				    "Cannot remove: organisation must have at least one lead.");
			  }
			  else
			  {
				  const auto             org_teams = m_kdb.teams_for_org(m_org_id);
				  std::vector<long long> member_team_ids;
				  for(const auto& t: org_teams)
				  {
					  if(m_kdb.is_member(t.id, uid))
					  {
						  member_team_ids.push_back(t.id);
					  }
				  }

				  m_odb.push_notification(
				    uid, "org_removed", make_org_event_payload(m_org_id, m_org_name));
				  m_kdb.remove_member_from_org_teams(m_org_id, uid);
				  m_invite_msg->setText("");

				  live_hub::instance().broadcast("user:" + uid);
				  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
				  for(const auto tid: member_team_ids)
				  {
					  live_hub::instance().broadcast("team:" + std::to_string(tid));
				  }

				  refresh_teams();
			  }
			  refresh_members();
		  });
	}
}

void kanban_team_page::refresh_pending()
{
	m_pending_section->clear();
	const auto pending = m_odb.org_pending(m_org_id);
	if(pending.empty())
	{
		m_pending_section->addNew<Wt::WText>("No pending invites.",
		                                     Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
		return;
	}
	for(const auto& p: pending)
	{
		auto* row = m_pending_section->addNew<Wt::WContainerWidget>();
		row->setStyleClass("kb-member-row");

		row->addNew<Wt::WText>(p.username + (p.is_lead ? " (invited as lead)" : ""),
		                       Wt::TextFormat::Plain)
		  ->setStyleClass("kb-member-name");

		auto* withdraw = row->addNew<Wt::WPushButton>("Withdraw");
		withdraw->setStyleClass("link-action-btn link-delete-btn");
		withdraw->clicked().connect(
		  [this, uid = p.username] {
			  m_odb.rescind_invite_notification(m_org_id, uid);
			  live_hub::instance().broadcast("user:" + uid);
			  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
			  m_odb.remove_org_member(m_org_id, uid);
			  refresh_pending();
		  });
	}
}

void kanban_team_page::refresh_teams()
{
	m_teams_section->clear();
	const auto teams = m_kdb.teams_for_org(m_org_id);
	if(teams.empty())
	{
		m_teams_section->addNew<Wt::WText>("No teams yet.", Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
		return;
	}
	for(const auto& t: teams)
	{
		build_team_block(m_teams_section, t);
	}
}

void kanban_team_page::build_team_block(Wt::WContainerWidget* parent,
                                        const team_entry&     team)
{
	auto* block = parent->addNew<Wt::WContainerWidget>();
	block->setStyleClass("kb-team-block");

	// Team header with rename.
	auto* hdr = block->addNew<Wt::WContainerWidget>();
	hdr->setStyleClass("kb-team-name-row");

	auto* name_input = hdr->addNew<Wt::WLineEdit>();
	name_input->setText(team.name);
	name_input->setStyleClass("editor-field");

	auto* rename_btn = hdr->addNew<Wt::WPushButton>("Rename");
	rename_btn->setStyleClass("editor-btn");
	rename_btn->clicked().connect(
	  [this, tid = team.id, name_input] {
		  const std::string n = name_input->text().toUTF8();
		  if(!n.empty())
		  {
			  m_kdb.rename_team(tid, n);
			  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
			  live_hub::instance().broadcast("team:" + std::to_string(tid));
		  }
	  });

	auto* del_team = hdr->addNew<Wt::WPushButton>("Delete team");
	del_team->setStyleClass("link-action-btn link-delete-btn");
	del_team->clicked().connect(
	  [this, tid = team.id] {
		  m_kdb.delete_team(tid);
		  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
		  live_hub::instance().broadcast("team:" + std::to_string(tid));
		  refresh_teams();
	  });

	// Members of this team.
	auto* mem_list = block->addNew<Wt::WContainerWidget>();
	mem_list->setStyleClass("kb-members-container");

	const auto members = m_kdb.team_member_entries(team.id);
	for(const auto& m: members)
	{
		auto* row = mem_list->addNew<Wt::WContainerWidget>();
		row->setStyleClass("kb-member-row");

		row->addNew<Wt::WText>(m.username + (m.is_lead ? " (lead)" : ""),
		                       Wt::TextFormat::Plain)
		  ->setStyleClass("kb-member-name");

		const std::string lead_label = m.is_lead ? "Remove lead" : "Make lead";
		auto*             lead_btn   = row->addNew<Wt::WPushButton>(lead_label);
		lead_btn->setStyleClass("link-action-btn");
		lead_btn->clicked().connect(
		  [this, tid = team.id, tname = team.name, uid = m.username, is_lead = m.is_lead] {
			  m_kdb.set_team_lead(tid, uid, !is_lead);
			  const std::string type =
			    is_lead ? "team_lead_demoted" : "team_lead_promoted";
			  m_odb.push_notification(uid, type, make_team_lead_payload(tid, tname));
			  live_hub::instance().broadcast("user:" + uid);
			  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
			  live_hub::instance().broadcast("team:" + std::to_string(tid));
			  refresh_teams();
		  });

		auto* rem = row->addNew<Wt::WPushButton>("Remove");
		rem->setStyleClass("link-action-btn link-delete-btn");
		rem->clicked().connect(
		  [this, tid = team.id, tname = team.name, uid = m.username] {
			  m_kdb.remove_member(tid, uid);
			  m_odb.push_notification(
			    uid, "team_removed", make_team_event_payload(tid, tname, m_org_id, m_org_name));
			  live_hub::instance().broadcast("user:" + uid);
			  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
			  live_hub::instance().broadcast("team:" + std::to_string(tid));
			  refresh_teams();
		  });
	}

	// Add-member-to-team form — only org members not already on this team.
	const auto               org_members = m_odb.org_members(m_org_id);
	std::vector<std::string> available;
	for(const auto& om: org_members)
	{
		bool already_on = false;
		for(const auto& tm: members)
		{
			if(tm.username == om.username)
			{
				already_on = true;
				break;
			}
		}
		if(!already_on)
		{
			available.push_back(om.username);
		}
	}

	auto* add_row = block->addNew<Wt::WContainerWidget>();
	add_row->setStyleClass("kb-member-add-row");

	if(available.empty())
	{
		add_row->addNew<Wt::WText>("All org members are on this team.",
		                           Wt::TextFormat::Plain)
		  ->setStyleClass("kb-team-note");
	}
	else
	{
		auto* combo = add_row->addNew<Wt::WComboBox>();
		combo->setStyleClass("gv-range-select");
		for(const auto& u: available)
		{
			combo->addItem(u);
		}

		auto* add_btn = add_row->addNew<Wt::WPushButton>("Add to team");
		add_btn->setStyleClass("editor-btn editor-btn-cancel");
		add_btn->clicked().connect(
		  [this, tid = team.id, tname = team.name, combo] {
			  const std::string u = combo->currentText().toUTF8();
			  if(u.empty())
			  {
				  return;
			  }
			  m_kdb.add_member(tid, u);
			  m_odb.push_notification(
			    u, "team_added", make_team_event_payload(tid, tname, m_org_id, m_org_name));
			  live_hub::instance().broadcast("user:" + u);
			  live_hub::instance().broadcast("org:" + std::to_string(m_org_id));
			  live_hub::instance().broadcast("team:" + std::to_string(tid));
			  refresh_teams();
		  });
	}
}
