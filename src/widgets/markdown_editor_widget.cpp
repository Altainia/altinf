#include "markdown_editor_widget.hpp"

#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WString.h>

markdown_editor_widget::markdown_editor_widget(const std::string& initial, bool view_mode):
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

	const auto        js_md        = Wt::WString::fromUTF8(initial).jsStringLiteral('"');
	const std::string js_view_mode = view_mode ? "true" : "false";

	// Single widget that manages both viewer and editor internally.
	//
	// view_mode=true  – starts as TUI Viewer; click enters editor; clicking outside
	//                   the widget returns to viewer.  Used for existing-task
	//                   description fields.
	// view_mode=false – always-edit mode (default).  Used for new tasks and comments.
	//
	// Several quirks of TUI Editor inside Wt dialogs are handled here:
	//
	// 1. focusout on the editing area must NOT collapse a view-mode editor.  Clicking
	//    the editor's own non-focusable chrome (toolbar gaps, the Markdown/WYSIWYG
	//    mode switch, the empty content area below the text) moves focus off the
	//    ProseMirror without landing on a focusable element, so relatedTarget /
	//    document.activeElement are outside the mount.  We therefore drive the
	//    view→viewer collapse from a "mousedown outside the widget" listener instead
	//    of from focusout, and use focusout only to sync the value back to Wt.
	//
	// 2. Wt dispatches a synthetic non-trusted click on `document` after every user
	//    interaction.  TUI's `handleClickDocument` sees target===document (neither
	//    `.more` nor `.dropdown-toolbar`) and would close the More dropdown the moment
	//    it opens.  We swallow ONLY the synthetic click that follows a click on a More
	//    button; later synthetic/real clicks pass through so the dropdown still closes
	//    normally when the user clicks away.
	doJavaScript(
	  "(function(mId,cbId,initMd,viewMode){"
	  "  var c=document.getElementById(mId);"
	  "  var cb=document.getElementById(cbId);"
	  "  if(!c||!cb)return;"
	  "  c._mdValue=initMd;"
	  "  c._mdInEdit=false;"

	  // Once-per-page guard for the More dropdown (issue #2 above).  A capture-phase
	  // tracker records whether the most recent real click was on a More button; the
	  // guard then suppresses only the synthetic document click that immediately
	  // follows it (which would otherwise close the just-opened dropdown).  Real
	  // click-aways and any later synthetic clicks are left alone so TUI can close
	  // the dropdown as usual.
	  "  if(!window._mdDropdownGuard){"
	  "    window._mdDropdownGuard=true;"
	  "    document.addEventListener('click',function(e){"
	  "      if(e.isTrusted){"
	  "        window._mdLastClickOnMore=!!(e.target&&e.target.closest&&"
	  "          e.target.closest('.toastui-editor-toolbar-icons.more'));"
	  "      }"
	  "    },true);"
	  "    document.addEventListener('click',function(e){"
	  "      if(e.isTrusted)return;"
	  "      if(e.target!==document)return;"
	  "      if(!window._mdLastClickOnMore)return;"
	  "      var dds=document.querySelectorAll('.toastui-editor-dropdown-toolbar');"
	  "      for(var i=0;i<dds.length;i++){"
	  "        if(dds[i].style.display!=='none'){e.stopPropagation();break;}"
	  "      }"
	  "    },true);"
	  "  }"

	  // focusout handler – syncs the current markdown back to Wt when focus genuinely
	  // leaves the editor.  Never collapses the editor (issue #1): the view-mode
	  // collapse is handled by the mousedown-outside listener installed in showEditor.
	  "  function onFocusOut(e){"
	  "    if(e.relatedTarget&&c.contains(e.relatedTarget))return;"
	  "    if(!e.relatedTarget&&document.activeElement&&c.contains(document.activeElement))return;"
	  "    var v=c._mdEditor?c._mdEditor.getMarkdown():c._mdValue;"
	  "    c._mdValue=v;"
	  "    cb.value=v;"
	  "    cb.dispatchEvent(new Event('change'));"
	  "  }"

	  "  function showViewer(content){"
	  "    if(c._mdEditor){c._mdEditor.destroy();c._mdEditor=null;}"
	  "    if(c._mdOutside){"
	  "      document.removeEventListener('mousedown',c._mdOutside,true);"
	  "      c._mdOutside=null;"
	  "    }"
	  "    c._mdInEdit=false;"
	  "    c.innerHTML='';"
	  "    c._mdViewer=toastui.Editor.factory({"
	  "      el:c,viewer:true,initialValue:content||''"
	  "    });"
	  "  }"

	  "  function showEditor(){"
	  "    if(c._mdViewer){c._mdViewer.destroy();c._mdViewer=null;}"
	  "    c._mdInEdit=true;"
	  "    c.innerHTML='';"
	  "    c._mdEditor=new toastui.Editor({"
	  "      el:c,height:'auto',initialEditType:'wysiwyg',"
	  "      initialValue:c._mdValue,"
	  "      events:{"
	  "        change:function(){if(c._mdEditor)cb.value=c._mdEditor.getMarkdown();}"
	  "      }"
	  "    });"
	  "    c.addEventListener('focusout',onFocusOut);"
	  "    if(viewMode){"
	  // Collapse back to the viewer only when a real mousedown lands outside the
	  // whole widget.  Clicks anywhere inside the editor (toolbar, mode switch,
	  // empty content area, the More dropdown) keep it open.  Ignore Wt's synthetic
	  // (non-trusted) events so entering edit mode doesn't immediately collapse.
	  "      var root=c.parentElement||c;"
	  "      c._mdOutside=function(e){"
	  "        if(!e.isTrusted)return;"
	  "        if(root.contains(e.target))return;"
	  "        var v=c._mdEditor?c._mdEditor.getMarkdown():c._mdValue;"
	  "        c._mdValue=v;"
	  "        cb.value=v;"
	  "        cb.dispatchEvent(new Event('change'));"
	  "        showViewer(v);"
	  "      };"
	  "      document.addEventListener('mousedown',c._mdOutside,true);"
	  "    }"
	  "  }"

	  "  if(viewMode){"
	  "    showViewer(initMd);"
	  // Listen on the widget root, not the mount `c`.  An empty/short description
	  // collapses the TUI viewer (and `c`) to ~0 height, so clicks land on the
	  // root's padded clickable area (.kb-popup-display) rather than on `c`.
	  // Listening on the root catches the click in every case.
	  "    var root=c.parentElement||c;"
	  "    root.addEventListener('click',function(){"
	  "      if(!c._mdInEdit){showEditor();if(c._mdEditor)c._mdEditor.focus();}"
	  "    });"
	  "  } else {"
	  "    showEditor();"
	  "  }"

	  "})('" +
	  m_mount_id + "','" + cb_id + "'," + js_md + "," + js_view_mode + ");");
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
	  "if(el&&el._mdEditor){el._mdEditor.focus();}");
}

Wt::Signal<std::string>& markdown_editor_widget::changed()
{
	return m_changed;
}
