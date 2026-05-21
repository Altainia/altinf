#pragma once

#include "auth/permission.hpp"
#include "auth/session_data.hpp"
#include "blog/blog_post.hpp"

#include <Wt/WContainerWidget.h>

class blog_post_page: public Wt::WContainerWidget
{
public:
	blog_post_page(const blog_post& post, const session_data& session);
};
