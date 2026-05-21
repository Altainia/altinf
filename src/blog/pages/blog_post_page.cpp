#include "blog_post_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>
#include <cmark.h>

#include <fstream>
#include <sstream>

blog_post_page::blog_post_page(const blog_post& post, const session_data& session)
{
	setStyleClass("page blog-post-page");

	auto* header = addNew<Wt::WContainerWidget>();
	header->setStyleClass("post-header");

	header->addNew<Wt::WText>("<h1>" + post.title + "</h1>", Wt::TextFormat::UnsafeXHTML);

	std::string date_html =
	  "<span class=\"post-date\">Posted: " + post.date.toString("yyyy-MM-dd").toUTF8() + "</span>";
	if(post.last_modified)
	{
		date_html +=
		  " <span class=\"post-date post-date-modified\">Updated: " +
		  post.last_modified->toString("yyyy-MM-dd").toUTF8() + "</span>";
	}
	header->addNew<Wt::WText>(date_html, Wt::TextFormat::UnsafeXHTML);

	if(!post.tags.empty())
	{
		auto* tag_row = header->addNew<Wt::WContainerWidget>();
		tag_row->setStyleClass("tag-row");

		for(const auto& tag: post.tags)
		{
			auto* chip = tag_row->addNew<Wt::WAnchor>(
			  Wt::WLink{Wt::LinkType::InternalPath, "/blog"}, tag);
			chip->setStyleClass("tag-chip");
		}
	}

	if(session.permissions.has_any(permission::post_write))
	{
		auto* edit_link = header->addNew<Wt::WAnchor>(
		  Wt::WLink{Wt::LinkType::InternalPath, "/admin/edit/" + post.slug}, "Edit");
		edit_link->setStyleClass("post-edit-link");
	}

	// Read markdown body (after frontmatter)
	std::ifstream file{post.filepath};
	std::string   line;
	bool          in_frontmatter   = false;
	bool          past_frontmatter = false;
	std::string   body;

	while(std::getline(file, line))
	{
		if(!past_frontmatter)
		{
			if(line == "---" && !in_frontmatter)
			{
				in_frontmatter = true;
				continue;
			}
			if(line == "---" && in_frontmatter)
			{
				past_frontmatter = true;
				continue;
			}
			if(in_frontmatter)
			{
				continue;
			}
		}
		body += line + "\n";
	}

	char* const html    = cmark_markdown_to_html(body.c_str(), body.size(), CMARK_OPT_DEFAULT);
	auto*       content = addNew<Wt::WContainerWidget>();
	content->setStyleClass("post-content");
	content->addNew<Wt::WText>(html, Wt::TextFormat::UnsafeXHTML);
	free(html);
}
