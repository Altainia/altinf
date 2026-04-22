#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

struct user
{
	std::string username;
	std::string password_hash;
	long long   permissions{0};

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, username, "username");
		Wt::Dbo::field(a, password_hash, "password_hash");
		Wt::Dbo::field(a, permissions, "permissions");
	}
};
