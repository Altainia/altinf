#include "markdown_editor_widget.hpp"

#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WString.h>

markdown_editor_widget::markdown_editor_widget(const std::string& initial):
  m_value{initial}
{
	Wt::WApplication::instance()->require("js/toastui-editor.min.js?v=" BUILD_VERSION);
	Wt::WApplication::instance()->useStyleSheet(Wt::WLink("css/toastui-editor.min.css"));

	auto* mount = addNew<Wt::WContainerWidget>();
	m_mount_id  = mount->id();

	m_hidden = addNew<Wt::WLineEdit>();
	m_hidden->setStyleClass("kb-cb-hidden");
	const std::string& cb_id = m_hidden->id();
	m_hidden->changed().connect([this] {
		m_value = m_hidden->text().toUTF8();
		m_changed.emit(m_value);
	});

	const auto js_md = Wt::WString::fromUTF8(initial).jsStringLiteral('"');
	doJavaScript(
	  "var el=document.getElementById('" + m_mount_id +
	  "');"
	  "var cb=document.getElementById('" +
	  cb_id +
	  "');"
	  "el._editor=new toastui.Editor({"
	  "  el:el,height:'auto',initialEditType:'wysiwyg',"
	  "  initialValue:" +
	  js_md +
	  ","
	  "  events:{"
	  "    change:function(){cb.value=el._editor.getMarkdown();},"
	  "    blur:function(){cb.dispatchEvent(new Event('change'));}"
	  "  }"
	  "});");
}

const std::string& markdown_editor_widget::value() const
{
	return m_value;
}

void markdown_editor_widget::focus()
{
	doJavaScript(
	  "var el=document.getElementById('" + m_mount_id +
	  "');"
	  "if(el&&el._editor){el._editor.focus();}");
}

Wt::Signal<std::string>& markdown_editor_widget::changed()
{
	return m_changed;
}
