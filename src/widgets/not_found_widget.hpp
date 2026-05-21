#pragma once

#include <Wt/WContainerWidget.h>
#include <string>

class not_found_widget: public Wt::WContainerWidget
{
public:
    explicit not_found_widget(const std::string& msg = "Page not found.");
};
