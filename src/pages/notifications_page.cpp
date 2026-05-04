#include "notifications_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "org/org.hpp"
#include "widgets/live_hub.hpp"

notifications_page::notifications_page(org_db&               odb,
                                       const session_data&   session,
                                       std::function<void()> on_read):
  m_db{odb},
  m_session{session},
  m_on_read{std::move(on_read)}
{
	setStyleClass("page notifications-page");
	addNew<Wt::WText>("<h1>Notifications</h1>", Wt::TextFormat::UnsafeXHTML);
	m_list = addNew<Wt::WContainerWidget>();
	m_list->setStyleClass("notif-list");
	refresh();
}

// ---- helpers ----------------------------------------------------------------

// Appends a Dismiss button that marks nid read and refreshes.
void notifications_page::add_dismiss(Wt::WContainerWidget* parent, long long nid)
{
	auto* btns = parent->addNew<Wt::WContainerWidget>();
	btns->setStyleClass("notif-actions");
	auto* btn = btns->addNew<Wt::WPushButton>("Dismiss");
	btn->setStyleClass("editor-btn editor-btn-cancel");
	btn->clicked().connect(
	  [this, nid] {
		  m_db.mark_read(nid);
		  if(m_on_read)
		  {
			  m_on_read();
		  }
		  refresh();
	  });
}

// ---- refresh ----------------------------------------------------------------

void notifications_page::refresh()
{
	m_list->clear();

	const auto items = m_db.notifications_for_user(m_session.username);
	if(items.empty())
	{
		m_list->addNew<Wt::WText>("No notifications.", Wt::TextFormat::Plain)
		  ->setStyleClass("notif-empty");
		return;
	}

	for(const auto& n: items)
	{
		auto* row = m_list->addNew<Wt::WContainerWidget>();
		row->setStyleClass(
		  n.is_read ? "notif-row notif-row--read" : "notif-row notif-row--unread");

		auto* body = row->addNew<Wt::WContainerWidget>();
		body->setStyleClass("notif-body");

		// ── org_invite ────────────────────────────────────────────────────────
		if(n.type == "org_invite")
		{
			const long long   org_id    = json_long(n.payload, "org_id");
			const std::string org_name  = json_str(n.payload, "org_name");
			const bool        rescinded = json_long(n.payload, "rescinded") != 0;

			if(rescinded)
			{
				body->addNew<Wt::WText>(
				      "Your invitation to join \"" + org_name +
				        "\" has been rescinded.",
				      Wt::TextFormat::Plain)
				  ->setStyleClass("notif-msg");
				body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
				                        Wt::TextFormat::Plain)
				  ->setStyleClass("notif-time");

				if(!n.is_read)
				{
					auto* btns = row->addNew<Wt::WContainerWidget>();
					btns->setStyleClass("notif-actions");
					auto* ack = btns->addNew<Wt::WPushButton>("Acknowledge");
					ack->setStyleClass("editor-btn editor-btn-cancel");
					ack->clicked().connect(
					  [this, nid = n.id] {
						  m_db.mark_read(nid);
						  if(m_on_read)
						  {
							  m_on_read();
						  }
						  refresh();
					  });
				}
			}
			else
			{
				const std::string msg =
				  "You have been invited to join the organisation \"" + org_name +
				  "\".";
				body->addNew<Wt::WText>(msg, Wt::TextFormat::Plain)
				  ->setStyleClass("notif-msg");
				body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
				                        Wt::TextFormat::Plain)
				  ->setStyleClass("notif-time");

				if(!n.is_read)
				{
					auto* btns = row->addNew<Wt::WContainerWidget>();
					btns->setStyleClass("notif-actions");

					auto* accept = btns->addNew<Wt::WPushButton>("Accept");
					accept->setStyleClass("editor-btn");
					accept->clicked().connect(
					  [this, nid = n.id, org_id] {
						  m_db.accept_invite(org_id, m_session.username);
						  m_db.mark_read(nid);
						  live_hub::instance().broadcast("org:" + std::to_string(org_id));
						  live_hub::instance().broadcast("user:" + m_session.username);
						  if(m_on_read)
						  {
							  m_on_read();
						  }
						  refresh();
					  });

					auto* decline = btns->addNew<Wt::WPushButton>("Decline");
					decline->setStyleClass("editor-btn editor-btn-danger");
					decline->clicked().connect(
					  [this, nid = n.id, org_id] {
						  m_db.decline_invite(org_id, m_session.username);
						  m_db.mark_read(nid);
						  live_hub::instance().broadcast("org:" + std::to_string(org_id));
						  if(m_on_read)
						  {
							  m_on_read();
						  }
						  refresh();
					  });
				}
			}
		}
		// ── task_assigned ─────────────────────────────────────────────────────
		else if(n.type == "task_assigned")
		{
			const long long   task_id    = json_long(n.payload, "task_id");
			const std::string task_title = json_str(n.payload, "task_title");
			const long long   team_id    = json_long(n.payload, "team_id");
			const std::string team_name  = json_str(n.payload, "team_name");

			body->addNew<Wt::WText>("You were assigned task \"",
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WAnchor>(
			      Wt::WLink{Wt::LinkType::InternalPath,
			                "/board/" + std::to_string(team_id) + "/task/" +
			                  std::to_string(task_id) + "/edit"},
			      task_title)
			  ->setStyleClass("notif-link");
			body->addNew<Wt::WText>(
			      "\" in team " + team_name + " \xe2\x80\x94 " + n.created_at,
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");

			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── org_removed ───────────────────────────────────────────────────────
		else if(n.type == "org_removed")
		{
			const std::string org_name = json_str(n.payload, "org_name");
			body->addNew<Wt::WText>(
			      "You have been removed from the organisation \"" + org_name + "\".",
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-time");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── org_lead_promoted ─────────────────────────────────────────────────
		else if(n.type == "org_lead_promoted")
		{
			const std::string org_name = json_str(n.payload, "org_name");
			body->addNew<Wt::WText>(
			      "You have been promoted to lead of \"" + org_name + "\".",
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-time");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── org_lead_demoted ──────────────────────────────────────────────────
		else if(n.type == "org_lead_demoted")
		{
			const std::string org_name = json_str(n.payload, "org_name");
			body->addNew<Wt::WText>(
			      "You are no longer a lead of \"" + org_name + "\".",
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-time");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── team_added ────────────────────────────────────────────────────────
		else if(n.type == "team_added")
		{
			const long long   team_id   = json_long(n.payload, "team_id");
			const std::string team_name = json_str(n.payload, "team_name");
			const std::string org_name  = json_str(n.payload, "org_name");
			body->addNew<Wt::WText>("You have been added to team ",
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WAnchor>(
			      Wt::WLink{Wt::LinkType::InternalPath,
			                "/board/" + std::to_string(team_id)},
			      team_name)
			  ->setStyleClass("notif-link");
			body->addNew<Wt::WText>(
			      " in \"" + org_name + "\" \xe2\x80\x94 " + n.created_at,
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── team_removed ──────────────────────────────────────────────────────
		else if(n.type == "team_removed")
		{
			const std::string team_name = json_str(n.payload, "team_name");
			const std::string org_name  = json_str(n.payload, "org_name");
			body->addNew<Wt::WText>(
			      "You have been removed from team \"" + team_name + "\" in \"" +
			        org_name + "\".",
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-time");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── team_lead_promoted ────────────────────────────────────────────────
		else if(n.type == "team_lead_promoted")
		{
			const std::string team_name = json_str(n.payload, "team_name");
			body->addNew<Wt::WText>(
			      "You have been promoted to lead of team \"" + team_name + "\".",
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-time");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── team_lead_demoted ─────────────────────────────────────────────────
		else if(n.type == "team_lead_demoted")
		{
			const std::string team_name = json_str(n.payload, "team_name");
			body->addNew<Wt::WText>(
			      "You are no longer a lead of team \"" + team_name + "\".",
			      Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-time");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
		// ── unknown (future types) ────────────────────────────────────────────
		else
		{
			body->addNew<Wt::WText>(n.type + ": " + n.payload, Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			if(!n.is_read)
			{
				add_dismiss(row, n.id);
			}
		}
	}
}
