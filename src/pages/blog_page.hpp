#pragma once

#include "blog/blog_post.hpp"

#include <Wt/WContainerWidget.h>

#include <string>
#include <vector>

class blog_page: public Wt::WContainerWidget
{
public:
	explicit blog_page(const std::vector<blog_post>& posts);

private:
	const std::vector<blog_post>& m_posts;
	std::string                   m_active_tag;
	Wt::WContainerWidget*         m_post_list{nullptr};

	void render_list();
};
