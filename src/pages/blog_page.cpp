#include "blog_page.h"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>

blog_page::blog_page(const std::vector<blog_post>& posts):
  m_posts{posts}
{
	setStyleClass("page blog-page");
	addNew<Wt::WText>("<h1>Blog</h1>", Wt::TextFormat::UnsafeXHTML);

	m_post_list = addNew<Wt::WContainerWidget>();
	m_post_list->setStyleClass("post-list");

	render_list();
}

void blog_page::render_list()
{
	m_post_list->clear();

	if(!m_active_tag.empty())
	{
		auto* filter_bar = m_post_list->addNew<Wt::WContainerWidget>();
		filter_bar->setStyleClass("filter-bar");

		auto* label =
		  filter_bar->addNew<Wt::WText>("Filtering by tag: <strong>" + m_active_tag + "</strong>",
		                                Wt::TextFormat::UnsafeXHTML);
		label->setStyleClass("filter-label");

		auto* clear_btn = filter_bar->addNew<Wt::WPushButton>("Clear");
		clear_btn->setStyleClass("tag-chip clear-chip");
		clear_btn->clicked().connect([this] {
			m_active_tag.clear();
			render_list();
		});
	}

	for(const auto& post: m_posts)
	{
		if(!m_active_tag.empty())
		{
			const auto has_tag =
			  std::find(post.tags.begin(), post.tags.end(), m_active_tag) != post.tags.end();
			if(!has_tag)
				continue;
		}

		auto* item = m_post_list->addNew<Wt::WContainerWidget>();
		item->setStyleClass("post-item");

		auto* title_link = item->addNew<Wt::WAnchor>(
		  Wt::WLink{Wt::LinkType::InternalPath, "/blog/" + post.slug}, post.title);
		title_link->setStyleClass("post-title");

		item->addNew<Wt::WText>(" <span class=\"post-date\">" + post.date + "</span>",
		                        Wt::TextFormat::UnsafeXHTML);

		if(!post.tags.empty())
		{
			auto* tag_row = item->addNew<Wt::WContainerWidget>();
			tag_row->setStyleClass("tag-row");

			for(const auto& tag: post.tags)
			{
				const auto captured_tag = tag;
				auto*      chip         = tag_row->addNew<Wt::WPushButton>(tag);
				chip->setStyleClass("tag-chip");
				chip->clicked().connect([this, captured_tag] {
					m_active_tag = captured_tag;
					render_list();
				});
			}
		}
	}
}
