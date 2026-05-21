#include "org_admin_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

org_admin_page::org_admin_page(org_db& odb, const session_data& session):
  m_db{odb},
  m_creator{session.username}
{
	setStyleClass("page org-admin-page");

	addNew<Wt::WText>("<h1>Organizations</h1>", Wt::TextFormat::UnsafeXHTML);

	// ── Create form ───────────────────────────────────────────────────────────
	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("org-create-form");

	form->addNew<Wt::WText>("<h2>Create organization</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* row = form->addNew<Wt::WContainerWidget>();
	row->setStyleClass("kb-member-add-row");

	m_name_input = row->addNew<Wt::WLineEdit>();
	m_name_input->setPlaceholderText("Organization name");
	m_name_input->setStyleClass("editor-field");

	auto* create_btn = row->addNew<Wt::WPushButton>("Create");
	create_btn->setStyleClass("editor-btn");
	create_btn->clicked().connect([this] { create_org(); });

	m_status_msg = form->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status_msg->setStyleClass("editor-status");

	// ── Organization list ─────────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>All organizations</h2>", Wt::TextFormat::UnsafeXHTML);
	m_list = addNew<Wt::WContainerWidget>();
	m_list->setStyleClass("org-list");

	refresh_list();
}

void org_admin_page::create_org()
{
	const std::string name = m_name_input->text().toUTF8();
	if(name.empty())
	{
		m_status_msg->setText("Name is required.");
		return;
	}
	m_db.create_org(name, m_creator);
	m_name_input->setText("");
	m_status_msg->setText("Organization created.");
	refresh_list();
}

void org_admin_page::refresh_list()
{
	m_list->clear();
	const auto orgs = m_db.all_orgs();
	if(orgs.empty())
	{
		m_list->addNew<Wt::WText>("No organizations yet.", Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
		return;
	}
	for(const auto& o: orgs)
	{
		auto* row = m_list->addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-list-row");
		row->addNew<Wt::WAnchor>(
		     Wt::WLink{Wt::LinkType::InternalPath, "/org/" + std::to_string(o.id)},
		     o.name)
		  ->setStyleClass("org-list-link");
	}
}
