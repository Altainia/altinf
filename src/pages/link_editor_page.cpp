#include "link_editor_page.h"

#include <Wt/WApplication.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <ctime>
#include <stdexcept>
#include <string>

static std::string today_date()
{
	const std::time_t t  = std::time(nullptr);
	const std::tm*    tm = std::localtime(&t);
	char              buf[11];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
	return buf;
}

link_editor_page::link_editor_page(link_db*              db,
                                   const link_entry*     existing,
                                   std::function<void()> on_save):
  m_db{db},
  m_on_save{std::move(on_save)}
{
	if(existing)
	{
		m_existing = *existing;
	}

	setStyleClass("page link-editor-page");

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("post-editor-form");

	form->addNew<Wt::WText>(m_existing ? "<h2>Edit Link</h2>" : "<h2>New Link</h2>",
	                        Wt::TextFormat::UnsafeXHTML);

	m_url = form->addNew<Wt::WLineEdit>();
	m_url->setStyleClass("editor-field");
	m_url->setPlaceholderText("URL (required)");
	if(m_existing)
	{
		m_url->setText(m_existing->url);
	}

	m_title = form->addNew<Wt::WLineEdit>();
	m_title->setStyleClass("editor-field");
	m_title->setPlaceholderText("Title (required)");
	if(m_existing)
	{
		m_title->setText(m_existing->title);
	}

	m_description = form->addNew<Wt::WTextArea>();
	m_description->setStyleClass("editor-field link-desc-field");
	m_description->setPlaceholderText("Short description (optional)");
	if(m_existing)
	{
		m_description->setText(m_existing->description);
	}

	m_section = form->addNew<Wt::WLineEdit>();
	m_section->setStyleClass("editor-field");
	m_section->setPlaceholderText("Section (e.g. \"Claude Artifacts\")");
	if(m_existing)
	{
		m_section->setText(m_existing->section);
	}

	m_sort_order = form->addNew<Wt::WLineEdit>();
	m_sort_order->setStyleClass("editor-field");
	m_sort_order->setPlaceholderText("Sort order within section (default 0)");
	m_sort_order->setText(m_existing ? std::to_string(m_existing->sort_order) : "0");

	m_status = form->addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status->setStyleClass("editor-status");

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	auto* save_btn = btn_row->addNew<Wt::WPushButton>("Save");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect(this, &link_editor_page::save);

	auto* cancel_btn = btn_row->addNew<Wt::WPushButton>("Cancel");
	cancel_btn->setStyleClass("editor-btn editor-btn-cancel");
	cancel_btn->clicked().connect([] {
		Wt::WApplication::instance()->setInternalPath("/links", true);
	});
}

void link_editor_page::save()
{
	const auto url     = m_url->text().toUTF8();
	const auto title   = m_title->text().toUTF8();
	const auto section = m_section->text().toUTF8();

	if(url.empty())
	{
		m_status->setText("URL is required.");
		return;
	}
	if(title.empty())
	{
		m_status->setText("Title is required.");
		return;
	}
	if(section.empty())
	{
		m_status->setText("Section is required.");
		return;
	}

	int sort_order = 0;
	try
	{
		sort_order = std::stoi(m_sort_order->text().toUTF8());
	}
	catch(const std::exception&)
	{
		sort_order = 0;
	}

	link_entry e;
	e.url         = url;
	e.title       = title;
	e.description = m_description->text().toUTF8();
	e.section     = section;
	e.sort_order  = sort_order;
	e.added_date  = m_existing ? m_existing->added_date : today_date();

	if(m_existing)
	{
		e.id = m_existing->id;
		m_db->update(e);
	}
	else
	{
		m_db->add(e);
	}

	m_on_save();
}
