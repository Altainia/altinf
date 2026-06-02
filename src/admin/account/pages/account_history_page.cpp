#include "account_history_page.hpp"

#include <Wt/WText.h>

#include <string>
#include <utility>

namespace
{
	std::string event_label(const std::string& type)
	{
		if(type == "created")
		{
			return "Account created";
		}
		if(type == "display_name_changed")
		{
			return "Display name changed";
		}
		if(type == "permissions_changed")
		{
			return "Permissions changed";
		}
		if(type == "password_changed")
		{
			return "Password changed";
		}
		if(type == "password_set")
		{
			return "Password set";
		}
		if(type == "password_unset")
		{
			return "Password removed";
		}
		if(type == "google_linked")
		{
			return "Google linked";
		}
		if(type == "google_unlinked")
		{
			return "Google unlinked";
		}
		if(type == "deleted")
		{
			return "Account deleted";
		}
		if(type == "token_created")
		{
			return "API token created";
		}
		if(type == "token_renamed")
		{
			return "API token renamed";
		}
		if(type == "token_revoked")
		{
			return "API token revoked";
		}
		return type;
	}

	std::string field_label(const std::string& name)
	{
		if(name == "display_name")
		{
			return "Display name";
		}
		if(name == "permissions")
		{
			return "Permissions";
		}
		if(name == "google")
		{
			return "Google";
		}
		if(name == "token")
		{
			return "Token";
		}
		return name;
	}
} // namespace

account_history_page::account_history_page(user_db& db, std::string username):
  m_db{db},
  m_username{std::move(username)}
{
	setStyleClass("page account-history-page");

	auto* header = addNew<Wt::WContainerWidget>();
	header->setStyleClass("vertical-section");
	header->addNew<Wt::WText>("<h1>Account history</h1>", Wt::TextFormat::UnsafeXHTML);
	header->addNew<Wt::WText>(m_username, Wt::TextFormat::Plain)->setStyleClass("account-username");

	const auto id = m_db.user_id_for(m_username);
	if(!id)
	{
		addNew<Wt::WText>("User not found.", Wt::TextFormat::Plain);
		return;
	}

	const auto events = m_db.history_for_user(*id);
	if(events.empty())
	{
		addNew<Wt::WText>("No history.", Wt::TextFormat::Plain)->setStyleClass("account-tokens-empty");
		return;
	}

	auto* timeline = addNew<Wt::WContainerWidget>();
	timeline->setStyleClass("history-timeline");

	for(const auto& ev: events)
	{
		auto* entry = timeline->addNew<Wt::WContainerWidget>();
		entry->setStyleClass("history-entry");

		const auto actor = m_db.display_name_for_id(ev.actor_id);
		entry->addNew<Wt::WText>(ev.occurred_at + " — " + actor, Wt::TextFormat::Plain)
		  ->setStyleClass("history-meta");

		entry->addNew<Wt::WText>(event_label(ev.event_type), Wt::TextFormat::Plain)
		  ->setStyleClass("history-event");

		for(const auto& ch: ev.changes)
		{
			const auto old_v = ch.old_value.empty() ? std::string{"(unset)"} : ch.old_value;
			const auto new_v = ch.new_value.empty() ? std::string{"(unset)"} : ch.new_value;
			entry
			  ->addNew<Wt::WText>(field_label(ch.field_name) + ": " + old_v + " → " + new_v,
			                      Wt::TextFormat::Plain)
			  ->setStyleClass("history-change");
		}
	}
}
