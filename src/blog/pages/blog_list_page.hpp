#pragma once

#include <Wt/WContainerWidget.h>

#include <string>
#include <vector>

#include "blog/blog_post.hpp"

class blog_list_page: public Wt::WContainerWidget
{
public:
	explicit blog_list_page(const std::vector<blog_post>& posts);

private:
	const std::vector<blog_post>& m_posts;
	std::string                   m_active_tag;
	Wt::WContainerWidget*         m_post_list{nullptr};

	void render_list();
};