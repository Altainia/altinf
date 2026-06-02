#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

struct api_token
{
	std::string token_hash;
	std::string username; // retained denormalized; user_id is the canonical link
	long long   user_id{0};
	std::string name; // user-chosen label, set at generation and renameable

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, token_hash, "token_hash");
		Wt::Dbo::field(a, username, "username");
		Wt::Dbo::field(a, user_id, "user_id");
		Wt::Dbo::field(a, name, "name");
	}
};