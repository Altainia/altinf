#pragma once

#include "auth/session_data.hpp"
#include "links/link.hpp"

#include <Wt/WContainerWidget.h>

#include <functional>
#include <vector>

class links_page: public Wt::WContainerWidget
{
public:
	links_page(const std::vector<link_entry>& links,
	           const session_data&            session,
	           std::function<void(long long)> on_delete);

private:
	const std::vector<link_entry>& m_links;
	const session_data&            m_session;
	std::function<void(long long)> m_on_delete;

	void render();
};
