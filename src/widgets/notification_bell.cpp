#include "notification_bell.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

notification_bell::notification_bell(int unread_count)
{
	setStyleClass("nav-bell");

	auto* link = addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/notifications"});
	link->setStyleClass("nav-bell-link");
	link->addNew<Wt::WText>("&#9956;", Wt::TextFormat::UnsafeXHTML); // bell glyph

	m_badge = link->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_badge->setStyleClass("nav-bell-badge");

	set_count(unread_count);
}

void notification_bell::set_count(int unread_count)
{
	if(unread_count > 0)
	{
		m_badge->setText(std::to_string(unread_count));
		m_badge->setHidden(false);
	}
	else
	{
		m_badge->setText("");
		m_badge->setHidden(true);
	}
}
