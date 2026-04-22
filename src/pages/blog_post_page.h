#pragma once

#include "auth/permission.h"
#include "auth/session_data.h"
#include "blog/blog_post.h"

#include <Wt/WContainerWidget.h>

class blog_post_page: public Wt::WContainerWidget
{
public:
	blog_post_page(const blog_post& post, const session_data& session);
};
