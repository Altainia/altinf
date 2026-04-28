#include "notifications_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "org/org.hpp"

notifications_page::notifications_page(org_db& odb, const session_data& session):
  m_db{odb},
  m_session{session}
{
	setStyleClass("page notifications-page");
	addNew<Wt::WText>("<h1>Notifications</h1>", Wt::TextFormat::UnsafeXHTML);
	m_list = addNew<Wt::WContainerWidget>();
	m_list->setStyleClass("notif-list");
	refresh();
}

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
		row->setStyleClass(n.is_read ? "notif-row notif-row--read" : "notif-row notif-row--unread");

		auto* body = row->addNew<Wt::WContainerWidget>();
		body->setStyleClass("notif-body");

		if(n.type == "org_invite")
		{
			const long long   org_id   = json_long(n.payload, "org_id");
			const std::string org_name = json_str(n.payload, "org_name");
			const std::string msg =
			  "You have been invited to join the organisation \"" + org_name + "\".";

			body->addNew<Wt::WText>(msg, Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			body->addNew<Wt::WText>(" \xe2\x80\x94 " + n.created_at,
			                        Wt::TextFormat::Plain)
			  ->setStyleClass("notif-time");

			auto* btns = row->addNew<Wt::WContainerWidget>();
			btns->setStyleClass("notif-actions");

			if(!n.is_read)
			{
				auto* accept = btns->addNew<Wt::WPushButton>("Accept");
				accept->setStyleClass("editor-btn");
				accept->clicked().connect(
				  [this, nid = n.id, org_id] {
					  m_db.accept_invite(org_id, m_session.username);
					  m_db.mark_read(nid);
					  refresh();
				  });

				auto* decline = btns->addNew<Wt::WPushButton>("Decline");
				decline->setStyleClass("editor-btn editor-btn-danger");
				decline->clicked().connect(
				  [this, nid = n.id, org_id] {
					  m_db.decline_invite(org_id, m_session.username);
					  m_db.mark_read(nid);
					  refresh();
				  });
			}
		}
		else if(n.type == "task_assigned")
		{
			const long long   task_id    = json_long(n.payload, "task_id");
			const std::string task_title = json_str(n.payload, "task_title");
			const long long   team_id    = json_long(n.payload, "team_id");
			const std::string team_name  = json_str(n.payload, "team_name");

			body->addNew<Wt::WText>(
			      "You were assigned task \"", Wt::TextFormat::Plain)
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
				auto* btns = row->addNew<Wt::WContainerWidget>();
				btns->setStyleClass("notif-actions");
				auto* dismiss = btns->addNew<Wt::WPushButton>("Dismiss");
				dismiss->setStyleClass("editor-btn editor-btn-cancel");
				dismiss->clicked().connect(
				  [this, nid = n.id] {
					  m_db.mark_read(nid);
					  refresh();
				  });
			}
		}
		else
		{
			// Future notification types: show raw payload + dismiss.
			body->addNew<Wt::WText>(n.type + ": " + n.payload, Wt::TextFormat::Plain)
			  ->setStyleClass("notif-msg");
			if(!n.is_read)
			{
				auto* btns = row->addNew<Wt::WContainerWidget>();
				btns->setStyleClass("notif-actions");
				auto* dismiss = btns->addNew<Wt::WPushButton>("Dismiss");
				dismiss->setStyleClass("editor-btn editor-btn-cancel");
				dismiss->clicked().connect(
				  [this, nid = n.id] {
					  m_db.mark_read(nid);
					  refresh();
				  });
			}
		}
	}
}
