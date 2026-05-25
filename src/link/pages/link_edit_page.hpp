#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <optional>
#include <string>

#include "link/link.hpp"
#include "link/link_db.hpp"

class link_edit_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new link
	// existing != nullptr  ->  edit link
	link_edit_page(link_db* db, const link_entry* existing);

	Wt::Signal<> saved;

private:
	link_db*                  m_db;
	std::optional<link_entry> m_existing;
	Wt::WLineEdit*            m_url{nullptr};
	Wt::WLineEdit*            m_title{nullptr};
	Wt::WTextArea*            m_description{nullptr};
	Wt::WLineEdit*            m_section{nullptr};
	Wt::WLineEdit*            m_sort_order{nullptr};
	Wt::WText*                m_status{nullptr};

	void save();
};
