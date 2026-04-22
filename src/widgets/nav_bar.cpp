#include "nav_bar.h"

#include <Wt/WAnchor.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

nav_bar::nav_bar()
{
	setStyleClass("nav-bar");

	auto* brand = addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/"}, "AltInf");
	brand->setStyleClass("nav-brand");

	auto* links = addNew<Wt::WContainerWidget>();
	links->setStyleClass("nav-links");

	auto* home_link = links->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/"}, "Home");
	home_link->setStyleClass("nav-link");

	auto* blog_link = links->addNew<Wt::WAnchor>(
	  Wt::WLink{Wt::LinkType::InternalPath, "/blog"}, "Blog");
	blog_link->setStyleClass("nav-link");
}
