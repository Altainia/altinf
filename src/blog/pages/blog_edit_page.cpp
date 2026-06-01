#include "blog_edit_page.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WDate.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <ranges>
#include <sstream>

#include "blog/post_writer.hpp"
#include "paths.hpp"

blog_edit_page::blog_edit_page(const std::filesystem::path& posts_dir,
                               const blog_post*             existing):
  m_posts_dir{posts_dir},
  m_existing{existing}
{
	setStyleClass("page post-editor-page");

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("post-editor-form");

	const auto heading = m_existing ? std::string{"Edit Post"} : std::string{"New Post"};
	form->addNew<Wt::WText>("<h2>" + heading + "</h2>", Wt::TextFormat::UnsafeXHTML);

	m_title = form->addNew<Wt::WLineEdit>();
	m_title->setPlaceholderText("Title");
	m_title->setStyleClass("editor-field");

	m_tags = form->addNew<Wt::WLineEdit>();
	m_tags->setPlaceholderText("Tags (comma-separated)");
	m_tags->setStyleClass("editor-field");

	std::string initial_body;
	if(m_existing)
	{
		if(auto body = read_body(*m_existing))
		{
			initial_body = std::move(*body);
		}
		else
		{
			m_read_failed = true;
		}
	}
	m_body_editor = form->addNew<markdown_editor_widget>(initial_body);
	m_body_editor->setStyleClass("post-body-editor");

	m_status = form->addNew<Wt::WText>();
	m_status->setStyleClass("editor-status");

	if(m_read_failed)
	{
		m_status->setText(
		  "Could not read the existing post file; saving is disabled to avoid "
		  "overwriting it with empty content.");
	}

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	auto* save_btn = btn_row->addNew<Wt::WPushButton>("Save");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect(this, &blog_edit_page::save);

	const auto cancel_path =
	  m_existing ? paths::blog_view(m_existing->slug) : paths::blog_list();
	auto* cancel_btn = btn_row->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, cancel_path}, "Cancel");
	cancel_btn->setStyleClass("editor-btn editor-btn-cancel");

	if(m_existing)
	{
		m_title->setText(m_existing->title);

		const auto tags_str =
		  m_existing->tags |
		  std::views::join_with(std::string(", ")) |
		  std::ranges::to<std::string>();

		m_tags->setText(tags_str);
	}
}

void blog_edit_page::save()
{
	if(m_read_failed)
	{
		m_status->setText("Cannot save: the existing post file could not be read.");
		return;
	}

	const auto title = m_title->text().toUTF8();
	const auto tags  = m_tags->text().toUTF8();
	const auto body  = m_body_editor->value();

	if(title.empty())
	{
		m_status->setText("Title is required.");
		return;
	}

	m_status->setText("");

	std::filesystem::path    filepath;
	std::string              slug;
	Wt::WDate                date;
	std::optional<Wt::WDate> last_modified;

	if(m_existing)
	{
		filepath      = m_existing->filepath;
		slug          = m_existing->slug;
		date          = m_existing->date;
		last_modified = Wt::WDate::currentDate();
	}
	else
	{
		auto result = resolve_new_post(m_posts_dir, title);
		filepath    = result.filepath;
		slug        = result.slug;
		date        = Wt::WDate::currentDate();
	}

	if(!write_post_file(filepath, title, date, last_modified, tags, body))
	{
		m_status->setText("Failed to write file.");
		return;
	}

	saved.emit(slug);
}

std::optional<std::string> blog_edit_page::read_body(const blog_post& post)
{
	std::ifstream file{post.filepath};
	if(!file)
	{
		return std::nullopt;
	}

	std::string line;
	bool        in_frontmatter   = false;
	bool        past_frontmatter = false;
	std::string body;

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
	return body;
}
