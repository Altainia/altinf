#include "altinf_app.h"

#include <Wt/WApplication.h>
#include <Wt/WServer.h>

int main(int argc, char** argv)
{
	return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env) {
		return std::make_unique<altinf_app>(env);
	});
}
