#include "footer.h"

#include <Wt/WText.h>

footer::footer()
{
	setStyleClass("site-footer");
	addNew<Wt::WText>("&copy; altainia");
}
