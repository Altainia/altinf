#include "main_page.hpp"

#include <Wt/WText.h>

main_page::main_page()
{
	setStyleClass("page main-page");
	addNew<Wt::WText>("<h1>Hi, I'm Ben.</h1>", Wt::TextFormat::UnsafeXHTML);
	addNew<Wt::WText>(
	  "<p>I am a software developer specializing in modern C++, with additional experience"
	  " in C# and Java. I focus on writing clean, efficient code and getting the"
	  " details right.</p>"
	  "<p>Outside of work, I mentor a high school robotics team competing in"
	  " FIRST Robotics Competition.</p>"
	  "<p>This is my personal site — I write here occasionally and use it to share"
	  " what I'm working on.</p>"
	  "<p><a href=\"https://github.com/Altainia\" target=\"_blank\""
	  " rel=\"noopener noreferrer\">GitHub</a></p>",
	  Wt::TextFormat::UnsafeXHTML);
}
