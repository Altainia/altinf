#pragma once

#include "blog/blog_post.h"

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <filesystem>
#include <functional>
#include <string>

class post_editor_page: public Wt::WContainerWidget
{
public:
	// existing == nullptr  ->  new post
	// existing != nullptr  ->  edit post (slug fixed to avoid breaking URLs)
	post_editor_page(const std::filesystem::path&          posts_dir,
	                 const blog_post*                      existing,
	                 std::function<void(std::string slug)> on_save);

private:
	std::filesystem::path            m_posts_dir;
	const blog_post*                 m_existing{nullptr};
	std::function<void(std::string)> m_on_save;
	Wt::WLineEdit*                   m_title{nullptr};
	Wt::WLineEdit*                   m_date{nullptr};
	Wt::WLineEdit*                   m_tags{nullptr};
	Wt::WTextArea*                   m_body{nullptr};
	Wt::WText*                       m_status{nullptr};

	void               save();
	std::string        make_slug(const std::string& title) const;
	static std::string read_body(const blog_post& post);
};
