#include "links_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <map>
#include <string>
#include <vector>

#include "auth/permission.hpp"

links_page::links_page(const std::vector<link_entry>& links,
                       const session_data&            session,
                       std::function<void(long long)> on_delete):
  m_links{links},
  m_session{session},
  m_on_delete{std::move(on_delete)}
{
	setStyleClass("page links-page");
	render();
}

void links_page::render()
{
	const bool can_edit = m_session.permissions.has_any(permission::post_write);

	addNew<Wt::WText>("<h1>Links</h1>", Wt::TextFormat::UnsafeXHTML);

	if(can_edit)
	{
		auto* add_btn = addNew<Wt::WPushButton>("Add Link");
		add_btn->setStyleClass("editor-btn link-add-btn");
		add_btn->clicked().connect([] {
			Wt::WApplication::instance()->setInternalPath("/admin/links/new", true);
		});
	}

	// Group by section
	std::map<std::string, std::vector<const link_entry*>> by_section;
	for(const auto& e: m_links)
	{
		by_section[e.section].push_back(&e);
	}

	if(by_section.empty())
	{
		addNew<Wt::WText>("<p class=\"links-empty\">No links yet.</p>",
		                  Wt::TextFormat::UnsafeXHTML);
		return;
	}

	// Render "Claude Artifacts" first, then remaining sections alphabetically
	const std::string artifacts = "Claude Artifacts";
	auto              arts_it   = by_section.find(artifacts);

	auto render_section = [&](const std::string&                    name,
	                          const std::vector<const link_entry*>& entries) {
		auto* sec = addNew<Wt::WContainerWidget>();
		sec->setStyleClass("link-section");

		sec->addNew<Wt::WText>("<h2>" + name + "</h2>", Wt::TextFormat::UnsafeXHTML);

		auto* list = sec->addNew<Wt::WContainerWidget>();
		list->setStyleClass("link-list");

		for(const link_entry* e: entries)
		{
			auto* item = list->addNew<Wt::WContainerWidget>();
			item->setStyleClass("link-item");

			Wt::WLink external{e->url};
			external.setTarget(Wt::LinkTarget::NewWindow);
			auto* anchor = item->addNew<Wt::WAnchor>(external, e->title);
			anchor->setStyleClass("link-title");

			if(!e->description.empty())
			{
				auto* desc =
				  item->addNew<Wt::WText>(Wt::WString{e->description}, Wt::TextFormat::Plain);
				desc->setStyleClass("link-desc");
			}

			if(can_edit)
			{
				const long long link_id = e->id;
				auto*           ctrl    = item->addNew<Wt::WContainerWidget>();
				ctrl->setStyleClass("link-ctrl");

				auto* edit_anchor = ctrl->addNew<Wt::WAnchor>(
				  Wt::WLink{Wt::LinkType::InternalPath,
				            "/admin/links/edit/" + std::to_string(link_id)},
				  "Edit");
				edit_anchor->setStyleClass("link-action-link");

				auto* del_btn = ctrl->addNew<Wt::WPushButton>("Delete");
				del_btn->setStyleClass("link-action-btn link-delete-btn");

				auto* confirm = ctrl->addNew<Wt::WContainerWidget>();
				confirm->setStyleClass("link-confirm");
				confirm->hide();

				confirm->addNew<Wt::WText>("Delete this link? ", Wt::TextFormat::Plain);

				auto* yes_btn = confirm->addNew<Wt::WPushButton>("Yes");
				yes_btn->setStyleClass("link-action-btn link-delete-btn");

				auto* no_btn = confirm->addNew<Wt::WPushButton>("No");
				no_btn->setStyleClass("link-action-btn");

				del_btn->clicked().connect([del_btn, confirm] {
					del_btn->hide();
					confirm->show();
				});

				no_btn->clicked().connect([del_btn, confirm] {
					confirm->hide();
					del_btn->show();
				});

				yes_btn->clicked().connect([this, link_id] {
					m_on_delete(link_id);
				});
			}
		}
	};

	if(arts_it != by_section.end())
	{
		render_section(arts_it->first, arts_it->second);
		by_section.erase(arts_it);
	}

	for(const auto& [name, entries]: by_section)
	{
		render_section(name, entries);
	}
}
