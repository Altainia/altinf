#include "post_editor_page.h"

#include <Wt/WAnchor.h>
#include <Wt/WDate.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WText.h>
#include <cmark.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

post_editor_page::post_editor_page(const std::filesystem::path&          posts_dir,
                                   const blog_post*                      existing,
                                   std::function<void(std::string slug)> on_save):
  m_posts_dir{posts_dir},
  m_existing{existing},
  m_on_save{std::move(on_save)}
{
	setStyleClass("page post-editor-page");

	auto* form = addNew<Wt::WContainerWidget>();
	form->setStyleClass("post-editor-form");

	const auto heading = m_existing ? std::string{"Edit Post"} : std::string{"New Post"};
	form->addNew<Wt::WText>("<h2>" + heading + "</h2>", Wt::TextFormat::UnsafeXHTML);

	m_title = form->addNew<Wt::WLineEdit>();
	m_title->setPlaceholderText("Title");
	m_title->setStyleClass("editor-field");

	m_date = form->addNew<Wt::WLineEdit>();
	m_date->setPlaceholderText("Date (YYYY-MM-DD)");
	m_date->setStyleClass("editor-field");

	m_tags = form->addNew<Wt::WLineEdit>();
	m_tags->setPlaceholderText("Tags (comma-separated)");
	m_tags->setStyleClass("editor-field");

	auto* tab_bar = form->addNew<Wt::WContainerWidget>();
	tab_bar->setStyleClass("editor-tab-bar");
	auto* write_tab   = tab_bar->addNew<Wt::WPushButton>("Write");
	auto* preview_tab = tab_bar->addNew<Wt::WPushButton>("Preview");
	write_tab->setStyleClass("editor-tab editor-tab-active");
	preview_tab->setStyleClass("editor-tab");

	m_stack = form->addNew<Wt::WStackedWidget>();
	m_body  = m_stack->addNew<Wt::WTextArea>();
	m_body->setStyleClass("editor-textarea");
	m_preview = m_stack->addNew<Wt::WContainerWidget>();
	m_preview->setStyleClass("editor-preview post-content");
	m_stack->setCurrentIndex(0);

	write_tab->clicked().connect([this, write_tab, preview_tab] {
		m_stack->setCurrentIndex(0);
		write_tab->setStyleClass("editor-tab editor-tab-active");
		preview_tab->setStyleClass("editor-tab");
	});

	preview_tab->clicked().connect([this, write_tab, preview_tab] {
		const auto  md   = m_body->text().toUTF8();
		char* const html = cmark_markdown_to_html(md.c_str(), md.size(), CMARK_OPT_DEFAULT);
		m_preview->clear();
		m_preview->addNew<Wt::WText>(std::string{html}, Wt::TextFormat::UnsafeXHTML);
		free(html);
		m_stack->setCurrentIndex(1);
		preview_tab->setStyleClass("editor-tab editor-tab-active");
		write_tab->setStyleClass("editor-tab");
	});

	m_status = form->addNew<Wt::WText>();
	m_status->setStyleClass("editor-status");

	auto* btn_row = form->addNew<Wt::WContainerWidget>();
	btn_row->setStyleClass("editor-btn-row");

	auto* save_btn = btn_row->addNew<Wt::WPushButton>("Save");
	save_btn->setStyleClass("editor-btn");
	save_btn->clicked().connect(this, &post_editor_page::save);

	const auto cancel_path =
	  m_existing ? std::string{"/blog/"} + m_existing->slug : std::string{"/blog"};
	auto* cancel_btn = btn_row->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, cancel_path}, "Cancel");
	cancel_btn->setStyleClass("editor-btn editor-btn-cancel");

	if(m_existing)
	{
		m_title->setText(m_existing->title);
		m_date->setText(m_existing->date);

		std::string tags_str;
		for(std::size_t i = 0; i < m_existing->tags.size(); ++i)
		{
			if(i > 0)
				tags_str += ", ";
			tags_str += m_existing->tags[i];
		}
		m_tags->setText(tags_str);

		m_body->setText(read_body(*m_existing));
	}
	else
	{
		m_date->setText(Wt::WDate::currentDate().toString("yyyy-MM-dd").toUTF8());
	}
}

void post_editor_page::save()
{
	const auto title = m_title->text().toUTF8();
	const auto date  = m_date->text().toUTF8();
	const auto tags  = m_tags->text().toUTF8();
	const auto body  = m_body->text().toUTF8();

	if(title.empty())
	{
		m_status->setText("Title is required.");
		return;
	}
	if(date.empty())
	{
		m_status->setText("Date is required.");
		return;
	}

	m_status->setText("");

	std::filesystem::path filepath;
	std::string           slug;

	if(m_existing)
	{
		filepath = m_existing->filepath;
		slug     = m_existing->slug;
	}
	else
	{
		slug             = make_slug(title);
		std::string base = date + "-" + slug;
		filepath         = m_posts_dir / (base + ".md");

		if(std::filesystem::exists(filepath))
		{
			for(int n = 2;; ++n)
			{
				const auto candidate = m_posts_dir / (base + "-" + std::to_string(n) + ".md");
				if(!std::filesystem::exists(candidate))
				{
					filepath = candidate;
					slug     = slug + "-" + std::to_string(n);
					break;
				}
			}
		}
	}

	std::ofstream out{filepath};
	if(!out)
	{
		m_status->setText("Failed to write file.");
		return;
	}

	out << "---\n";
	out << "title: " << title << "\n";
	out << "date: " << date << "\n";
	out << "tags: " << tags << "\n";
	out << "---\n";
	out << body;

	out.flush();

	m_on_save(slug);
}

std::string post_editor_page::make_slug(const std::string& title) const
{
	std::string slug;
	for(const char c: title)
	{
		if(std::isalnum(static_cast<unsigned char>(c)))
			slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		else if(std::isspace(static_cast<unsigned char>(c)) && !slug.empty() && slug.back() != '-')
			slug += '-';
	}
	while(!slug.empty() && slug.back() == '-')
		slug.pop_back();
	return slug.empty() ? "post" : slug;
}

std::string post_editor_page::read_body(const blog_post& post)
{
	std::ifstream file{post.filepath};
	std::string   line;
	bool          in_fm   = false;
	bool          past_fm = false;
	std::string   body;

	while(std::getline(file, line))
	{
		if(!past_fm)
		{
			if(line == "---" && !in_fm)
			{
				in_fm = true;
				continue;
			}
			if(line == "---" && in_fm)
			{
				past_fm = true;
				continue;
			}
			if(in_fm)
				continue;
		}
		body += line + "\n";
	}

	return body;
}
