#pragma once

#include "blog/blog_post.h"

#include <Wt/WContainerWidget.h>

class blog_post_page: public Wt::WContainerWidget
{
public:
	explicit blog_post_page(const blog_post& post);
};
