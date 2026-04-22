#pragma once

#include "links/link.h"
#include "links/link_db.h"

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <functional>
#include <optional>
#include <string>

class link_editor_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new link
	// existing != nullptr  ->  edit link
	link_editor_page(link_db*              db,
	                 const link_entry*     existing,
	                 std::function<void()> on_save);

private:
	link_db*                  m_db;
	std::optional<link_entry> m_existing;
	std::function<void()>     m_on_save;
	Wt::WLineEdit*            m_url{nullptr};
	Wt::WLineEdit*            m_title{nullptr};
	Wt::WTextArea*            m_description{nullptr};
	Wt::WLineEdit*            m_section{nullptr};
	Wt::WLineEdit*            m_sort_order{nullptr};
	Wt::WText*                m_status{nullptr};

	void save();
};
