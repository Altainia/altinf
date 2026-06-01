#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>
#include <Wt/WTextArea.h>

#include <string>

class markdown_editor_widget: public Wt::WContainerWidget
{
public:
	// view_mode = true : starts as a clickable viewer; click enters edit mode,
	//                    focusout returns to viewer.  Used for existing-task fields.
	// view_mode = false: always-edit mode (default).  Used for new tasks and comments.
	explicit markdown_editor_widget(const std::string& initial = "", bool view_mode = false);

	// Last value synced from the editor (updated when focus leaves the editor).
	const std::string& value() const;

	// Move keyboard focus into the TUI editor surface (always-edit mode).
	void focus();

	// Fires when focus leaves the editor, carrying the current markdown string.
	Wt::Signal<std::string>& changed();

private:
	std::string             m_mount_id;
	std::string             m_value;
	Wt::WTextArea*          m_hidden{nullptr};
	Wt::Signal<std::string> m_changed;
};
