#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>

#include <vector>

#include "auth/session_data.hpp"
#include "link/link.hpp"

class link_list_page: public Wt::WContainerWidget
{
public:
	link_list_page(const std::vector<link_entry>& links, const session_data& session);

	Wt::Signal<long long> deleted;

private:
	std::vector<link_entry> m_links;
	session_data            m_session;

	void render();
};
