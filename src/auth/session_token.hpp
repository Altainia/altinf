#pragma once

#include <Wt/Dbo/Dbo.h>

#include <string>

struct session_token
{
	std::string token_hash;
	long long   user_id{0}; // owning user (FK to user.id)

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, token_hash, "token_hash");
		Wt::Dbo::field(a, user_id, "user_id");
	}
};