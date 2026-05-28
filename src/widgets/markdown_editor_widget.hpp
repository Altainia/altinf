#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WSignal.h>

#include <string>

class markdown_editor_widget: public Wt::WContainerWidget
{
public:
	explicit markdown_editor_widget(const std::string& initial = "");

	// Last value synced from the editor (updated when the editor loses focus).
	const std::string& value() const;

	// Call after showing this widget (e.g. entering edit mode) to
	// move keyboard focus into the TUI editor surface.
	void focus();

	// Fires when the editor loses focus, carrying the current markdown string.
	Wt::Signal<std::string>& changed();

private:
	std::string             m_mount_id;
	std::string             m_value;
	Wt::WLineEdit*          m_hidden{nullptr};
	Wt::Signal<std::string> m_changed;
};
