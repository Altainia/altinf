#pragma once

#include "blog/blog_post.h"

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>

#include <vector>

class altinf_app: public Wt::WApplication
{
public:
	explicit altinf_app(const Wt::WEnvironment& env);

private:
	std::vector<blog_post> m_posts;
	Wt::WContainerWidget*  m_content{nullptr};

	void handle_path(const std::string& path);
};
