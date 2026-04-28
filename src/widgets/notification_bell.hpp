#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>

class notification_bell: public Wt::WContainerWidget
{
public:
	explicit notification_bell(int unread_count);

	void set_count(int unread_count);

private:
	Wt::WText* m_badge{nullptr};
};
