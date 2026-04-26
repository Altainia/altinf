#include "footer.hpp"

#include <Wt/WText.h>

footer::footer()
{
	setStyleClass("site-footer");
	addNew<Wt::WText>("&copy; altainia");
}
