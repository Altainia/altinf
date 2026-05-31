#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WSignal.h>
#include <Wt/WText.h>

#include <filesystem>
#include <string>

#include "blog/blog_post.hpp"
#include "widgets/markdown_editor_widget.hpp"

class blog_edit_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new post
	// existing != nullptr  ->  edit post (slug fixed to avoid breaking URLs)
	blog_edit_page(const std::filesystem::path& posts_dir,
	               const blog_post*             existing);

	Wt::Signal<std::string> saved;

private:
	std::filesystem::path   m_posts_dir;
	const blog_post*        m_existing{nullptr};
	Wt::WLineEdit*          m_title{nullptr};
	Wt::WLineEdit*          m_tags{nullptr};
	markdown_editor_widget* m_body_editor{nullptr};
	Wt::WText*              m_status{nullptr};

	void               save();
	static std::string read_body(const blog_post& post);
};
