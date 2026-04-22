#include "altinf_app.h"

#include "blog/blog_loader.h"
#include "pages/blog_page.h"
#include "pages/blog_post_page.h"
#include "pages/main_page.h"
#include "widgets/footer.h"
#include "widgets/nav_bar.h"

#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <algorithm>
#include <filesystem>

altinf_app::altinf_app(const Wt::WEnvironment& env):
  Wt::WApplication{env}
{
	setTitle("AltInf");
	useStyleSheet(Wt::WLink{"css/altinf.css"});

	const auto posts_dir = std::filesystem::path{appRoot()} / "posts";
	m_posts              = blog_loader{posts_dir}.load();

	root()->setStyleClass("site-root");
	root()->addNew<nav_bar>();
	m_content = root()->addNew<Wt::WContainerWidget>();
	m_content->setStyleClass("site-content");
	root()->addNew<footer>();

	internalPathChanged().connect([this](const std::string& path) {
		handle_path(path);
	});

	handle_path(internalPath());
}

void altinf_app::handle_path(const std::string& path)
{
	m_content->clear();

	if(path == "/" || path.empty())
	{
		m_content->addNew<main_page>();
	}
	else if(path == "/blog")
	{
		m_content->addNew<blog_page>(m_posts);
	}
	else if(path.substr(0, 6) == "/blog/")
	{
		const auto slug = path.substr(6);
		const auto it   = std::find_if(m_posts.begin(), m_posts.end(), [&slug](const blog_post& p) {
			return p.slug == slug;
		});

		if(it != m_posts.end())
			m_content->addNew<blog_post_page>(*it);
		else
			m_content->addNew<Wt::WText>("Post not found.", Wt::TextFormat::Plain);
	}
	else
	{
		m_content->addNew<Wt::WText>("Page not found.", Wt::TextFormat::Plain);
	}
}
