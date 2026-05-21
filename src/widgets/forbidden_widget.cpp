#include "forbidden_widget.hpp"

#include <Wt/WText.h>

forbidden_widget::forbidden_widget()
{
	setStyleClass("error-page error-page--forbidden");
	addNew<Wt::WText>("Forbidden.", Wt::TextFormat::Plain);
}
