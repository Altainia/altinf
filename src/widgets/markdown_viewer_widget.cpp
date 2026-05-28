#include "markdown_viewer_widget.hpp"

#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WString.h>

markdown_viewer_widget::markdown_viewer_widget(const std::string& markdown)
{
	Wt::WApplication::instance()->require("js/toastui-editor-viewer.min.js?v=" BUILD_VERSION);
	Wt::WApplication::instance()->useStyleSheet(Wt::WLink("css/toastui-editor.min.css"));

	auto* mount = addNew<Wt::WContainerWidget>();
	m_mount_id  = mount->id();

	const auto js_md = Wt::WString::fromUTF8(markdown).jsStringLiteral('"');
	doJavaScript(
	  "var el=document.getElementById('" + m_mount_id +
	  "');"
	  "el._viewer=new toastui.Viewer({el:el,initialValue:" +
	  js_md + "});");
}

void markdown_viewer_widget::set_content(const std::string& markdown)
{
	const auto js_md = Wt::WString::fromUTF8(markdown).jsStringLiteral('"');
	doJavaScript(
	  "var el=document.getElementById('" + m_mount_id +
	  "');"
	  "if(el&&el._viewer){el._viewer.setMarkdown(" +
	  js_md + ");}");
}
