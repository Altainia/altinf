#include "main_page.h"

#include <Wt/WText.h>

main_page::main_page()
{
	setStyleClass("page main-page");
	addNew<Wt::WText>("<h1>Hello.</h1>", Wt::TextFormat::UnsafeXHTML);
	addNew<Wt::WText>(
	  "<p>Welcome to AltInf — my personal corner of the web.</p>",
	  Wt::TextFormat::UnsafeXHTML);
}
