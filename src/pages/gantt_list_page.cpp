#include "gantt_list_page.h"

#include "auth/permission.h"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

gantt_list_page::gantt_list_page(const std::vector<gantt_project_entry>& projects,
                                 const session_data&                     session)
{
	setStyleClass("page gantt-list-page");

	addNew<Wt::WText>("<h1>Gantt Charts</h1>", Wt::TextFormat::UnsafeXHTML);

	if(has_permission(session.permissions, permission::gantt_write) ||
	   has_permission(session.permissions, permission::admin))
	{
		auto* new_btn = addNew<Wt::WAnchor>(
		  Wt::WLink{Wt::LinkType::InternalPath, "/admin/gantt/new"}, "New Chart");
		new_btn->setStyleClass("gantt-new-btn editor-btn");
	}

	if(projects.empty())
	{
		addNew<Wt::WText>("No charts available.", Wt::TextFormat::Plain)
		  ->setStyleClass("gantt-empty");
		return;
	}

	auto* list = addNew<Wt::WContainerWidget>();
	list->setStyleClass("gantt-project-list");

	for(const auto& proj: projects)
	{
		auto* item = list->addNew<Wt::WContainerWidget>();
		item->setStyleClass("gantt-project-item");

		const auto path = "/gantt/" + std::to_string(proj.id);
		auto*      link = item->addNew<Wt::WAnchor>(
      Wt::WLink{Wt::LinkType::InternalPath, path}, proj.title);
		link->setStyleClass("gantt-project-title");

		if(!proj.description.empty())
		{
			auto* desc = item->addNew<Wt::WText>(proj.description, Wt::TextFormat::Plain);
			desc->setStyleClass("gantt-project-desc");
		}

		auto* meta = item->addNew<Wt::WText>(
		  "Owner: " + proj.owner_username + "  ·  Created: " + proj.created_date,
		  Wt::TextFormat::Plain);
		meta->setStyleClass("gantt-project-meta");
	}
}
