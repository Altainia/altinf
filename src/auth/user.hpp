#pragma once

#include "permission.hpp"

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/SqlConnection.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/SqlTraits.h>

#include <string>

struct user
{
	std::string     username;
	std::string     display_name;
	std::string     password_hash;
	permission::flags permissions;

	template<class Action>
	void persist(Action& a)
	{
		Wt::Dbo::field(a, username, "username");
		Wt::Dbo::field(a, display_name, "display_name");
		Wt::Dbo::field(a, password_hash, "password_hash");
		Wt::Dbo::field(a, permissions, "permissions");
	}
};

namespace Wt::Dbo {

template<>
struct sql_value_traits<permission::flags, void>
{
	static std::string type(SqlConnection* conn, int size)
	{
		return sql_value_traits<long long>::type(conn, size);
	}

	static void bind(const permission::flags& v, SqlStatement* stmt, int col, int size)
	{
		stmt->bind(col, static_cast<long long>(v.value()));
	}

	static bool read(permission::flags& v, SqlStatement* stmt, int col, int size)
	{
		long long raw = 0;
		const bool ok = stmt->getResult(col, &raw);
		if(ok)
			v = permission::flags::from_value(static_cast<permission::flags::value_type>(raw));
		return ok;
	}
};

} // namespace Wt::Dbo
