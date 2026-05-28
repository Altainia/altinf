#pragma once

#include <Wt/WContainerWidget.h>

#include <string>

class markdown_viewer_widget: public Wt::WContainerWidget
{
public:
	explicit markdown_viewer_widget(const std::string& markdown = "");

	void set_content(const std::string& markdown);

private:
	std::string m_mount_id;
};
