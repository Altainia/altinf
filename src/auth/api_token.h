#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

struct api_token
{
	std::string token_hash;
	std::string username;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, token_hash, "token_hash");
		Wt::Dbo::field(a, username, "username");
	}
};
