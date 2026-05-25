#pragma once

#include <Wt/WContainerWidget.h>

#include <functional>
#include <vector>

#include "auth/session_data.hpp"
#include "link/link.hpp"

class link_list_page: public Wt::WContainerWidget
{
public:
	link_list_page(const std::vector<link_entry>& links,
	               const session_data&            session,
	               std::function<void(long long)> on_delete);

private:
	const std::vector<link_entry>& m_links;
	const session_data&            m_session;
	std::function<void(long long)> m_on_delete;

	void render();
};