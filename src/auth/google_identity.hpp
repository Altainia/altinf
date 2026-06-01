#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

// A logged-in user's linked Google account. The Google subject id (the stable
// `sub` claim) is the join key used at sign-in; email is kept for display only.
struct google_identity
{
	std::string username;
	std::string google_sub;
	std::string email;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, username, "username");
		Wt::Dbo::field(a, google_sub, "google_sub");
		Wt::Dbo::field(a, email, "email");
	}
};
