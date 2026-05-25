#pragma once

#include <Wt/WContainerWidget.h>

#include "auth/permission.hpp"
#include "auth/session_data.hpp"
#include "blog/blog_post.hpp"

class blog_view_page: public Wt::WContainerWidget
{
public:
	blog_view_page(const blog_post& post, const session_data& session);
};