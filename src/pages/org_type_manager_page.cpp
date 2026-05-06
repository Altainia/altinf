#include "org_type_manager_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WColor.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <cstdio>

#include "widgets/live_hub.hpp"

const std::vector<std::string> org_type_manager_page::k_palette = {
  "#e07b54",
  "#5a9e6f",
  "#7aa2d4",
  "#c47fc4",
  "#d4a04e",
  "#4eb8d4",
  "#d45a5a",
  "#7dc47d",
  "#8888cc",
  "#c4b84e"};

static Wt::WColor hex_to_wcolor(const std::string& hex)
{
	if(hex.size() != 7 || hex[0] != '#')
	{
		return Wt::WColor{0x7a, 0xa2, 0xd4};
	}
	try
	{
		int r = std::stoi(hex.substr(1, 2), nullptr, 16);
		int g = std::stoi(hex.substr(3, 2), nullptr, 16);
		int b = std::stoi(hex.substr(5, 2), nullptr, 16);
		return Wt::WColor{r, g, b};
	}
	catch(...)
	{}
	return Wt::WColor{0x7a, 0xa2, 0xd4};
}

static std::string wcolor_to_hex(const Wt::WColor& c)
{
	char buf[8];
	std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.red(), c.green(), c.blue());
	return buf;
}

org_type_manager_page::org_type_manager_page(kanban_db&          db,
                                             org_db&             odb,
                                             long long           org_id,
                                             const session_data& session):
  m_db{db},
  m_org_id{org_id}
{
	(void)session;
	setStyleClass("page org-type-manager-page");

	const auto        org      = odb.find_org(org_id);
	const std::string org_name = org ? org->name : "Organization";

	addNew<Wt::WText>("<h1>Task Types \xe2\x80\x94 " + org_name + "</h1>",
	                  Wt::TextFormat::UnsafeXHTML);

	const std::string back_url = "/org/" + std::to_string(org_id);
	addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, back_url},
	  "\xe2\x86\x90 Back to organization")
	  ->setStyleClass("kb-back-link");

	// ── Existing types list ───────────────────────────────────────────────
	m_list = addNew<Wt::WContainerWidget>();
	m_list->setStyleClass("org-type-list");
	refresh_list();

	// ── Add new type form ─────────────────────────────────────────────────
	addNew<Wt::WText>("<h2>Add new type</h2>", Wt::TextFormat::UnsafeXHTML);

	auto* add_row = addNew<Wt::WContainerWidget>();
	add_row->setStyleClass("kb-editor-row");

	m_name_input = add_row->addNew<Wt::WLineEdit>();
	m_name_input->setPlaceholderText("Type name");
	m_name_input->setStyleClass("editor-field");

	const auto        existing_types = m_db.types_for_org(m_org_id);
	const std::string default_color =
	  k_palette[existing_types.size() % k_palette.size()];

	m_color_picker = add_row->addNew<Wt::WColorPicker>();
	m_color_picker->setStyleClass("kb-color-picker");
	m_color_picker->setColor(hex_to_wcolor(default_color));

	auto* add_btn = add_row->addNew<Wt::WPushButton>("Add Type");
	add_btn->setStyleClass("editor-btn");
	add_btn->clicked().connect([this] { add_type(); });

	m_status_msg = addNew<Wt::WText>("", Wt::TextFormat::Plain);
	m_status_msg->setStyleClass("editor-status");
}

void org_type_manager_page::refresh_list()
{
	m_list->clear();
	m_editing_id = 0;
	m_edit_name  = nullptr;
	m_edit_color = nullptr;

	const auto types = m_db.types_for_org(m_org_id);
	if(types.empty())
	{
		m_list->addNew<Wt::WText>("No types yet.", Wt::TextFormat::Plain)
		  ->setStyleClass("org-empty-note");
		return;
	}

	for(const auto& ty: types)
	{
		auto* row = m_list->addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-type-row");

		auto* dot = row->addNew<Wt::WContainerWidget>();
		dot->setStyleClass("org-type-dot");
		dot->decorationStyle().setBackgroundColor(hex_to_wcolor(ty.color));

		row->addNew<Wt::WText>(ty.name, Wt::TextFormat::Plain)
		  ->setStyleClass("org-type-name");

		auto* edit_btn = row->addNew<Wt::WPushButton>("Edit");
		edit_btn->setStyleClass("editor-btn editor-btn-cancel");
		edit_btn->clicked().connect([this, id = ty.id] { start_edit(id); });

		auto* del_btn = row->addNew<Wt::WPushButton>("Delete");
		del_btn->setStyleClass("editor-btn editor-btn-danger");
		del_btn->clicked().connect([this, id = ty.id] { delete_type(id); });
	}
}

void org_type_manager_page::start_edit(long long type_id)
{
	const auto opt = m_db.find_task_type(type_id);
	if(!opt)
	{
		return;
	}

	m_editing_id = type_id;
	m_list->clear();

	const auto types = m_db.types_for_org(m_org_id);
	for(const auto& ty: types)
	{
		auto* row = m_list->addNew<Wt::WContainerWidget>();
		row->setStyleClass("org-type-row");

		if(ty.id == type_id)
		{
			m_edit_name = row->addNew<Wt::WLineEdit>();
			m_edit_name->setText(ty.name);
			m_edit_name->setStyleClass("editor-field");

			m_edit_color = row->addNew<Wt::WColorPicker>();
			m_edit_color->setStyleClass("kb-color-picker");
			m_edit_color->setColor(hex_to_wcolor(ty.color));

			auto* save_btn = row->addNew<Wt::WPushButton>("Save");
			save_btn->setStyleClass("editor-btn");
			save_btn->clicked().connect([this, type_id] { save_edit(type_id); });

			auto* cancel_btn = row->addNew<Wt::WPushButton>("Cancel");
			cancel_btn->setStyleClass("editor-btn editor-btn-cancel");
			cancel_btn->clicked().connect([this] { refresh_list(); });
		}
		else
		{
			auto* dot = row->addNew<Wt::WContainerWidget>();
			dot->setStyleClass("org-type-dot");
			dot->decorationStyle().setBackgroundColor(hex_to_wcolor(ty.color));
			row->addNew<Wt::WText>(ty.name, Wt::TextFormat::Plain)
			  ->setStyleClass("org-type-name");
		}
	}
}

void org_type_manager_page::save_edit(long long type_id)
{
	if(!m_edit_name || !m_edit_color)
	{
		return;
	}
	const std::string name = m_edit_name->text().toUTF8();
	if(name.empty())
	{
		m_status_msg->setText("Name is required.");
		return;
	}
	m_db.update_task_type(type_id, name, wcolor_to_hex(m_edit_color->color()));
	live_hub::instance().broadcast(
	  "org:" + std::to_string(m_org_id) + ":types");
	m_status_msg->setText("");
	refresh_list();
}

void org_type_manager_page::add_type()
{
	const std::string name = m_name_input->text().toUTF8();
	if(name.empty())
	{
		m_status_msg->setText("Name is required.");
		return;
	}
	m_db.create_task_type(
	  m_org_id, name, wcolor_to_hex(m_color_picker->color()));
	live_hub::instance().broadcast(
	  "org:" + std::to_string(m_org_id) + ":types");

	m_name_input->setText({});
	m_status_msg->setText("");

	const auto types = m_db.types_for_org(m_org_id);
	m_color_picker->setColor(
	  hex_to_wcolor(k_palette[types.size() % k_palette.size()]));

	refresh_list();
}

void org_type_manager_page::delete_type(long long type_id)
{
	m_db.delete_task_type(type_id);
	live_hub::instance().broadcast(
	  "org:" + std::to_string(m_org_id) + ":types");
	refresh_list();
}
