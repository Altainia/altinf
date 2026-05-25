#include "not_found_widget.hpp"

#include <Wt/WText.h>

not_found_widget::not_found_widget(const std::string& msg)
{
	setStyleClass("error-page error-page--not-found");
	addNew<Wt::WText>(msg, Wt::TextFormat::Plain);
}