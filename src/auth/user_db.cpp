#include "user_db.h"

#include <Wt/Auth/HashFunction.h>
#include <Wt/Dbo/Transaction.h>

user_db::user_db(const std::string& db_path):
  m_sqlite{db_path}
{
	m_dbo.setConnection(m_sqlite);
	m_dbo.mapClass<user>("user");

	try
	{
		m_dbo.createTables();
	}
	catch(const Wt::Dbo::Exception&)
	{
		// Tables already exist — ignore
	}
}

bool user_db::authenticate(const std::string& uname,
                           const std::string& password,
                           session_data&      out)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results = m_dbo.find<user>()
	                       .where("username = ?")
	                       .bind(uname)
	                       .resultList();

	if(results.empty())
		return false;

	const Wt::Dbo::ptr<user> found = *results.begin();

	const Wt::Auth::BCryptHashFunction bcrypt{12};
	if(!bcrypt.verify(password, "", found->password_hash))
		return false;

	out.logged_in   = true;
	out.username    = uname;
	out.permissions = found->permissions;

	return true;
}

void user_db::create_user(const std::string& uname,
                          const std::string& password,
                          uint64_t           perms)
{
	const Wt::Auth::BCryptHashFunction bcrypt{12};
	const std::string                  hash = bcrypt.compute(password, "");

	Wt::Dbo::Transaction t{m_dbo};
	auto                 new_user    = m_dbo.add(std::make_unique<user>());
	new_user.modify()->username      = uname;
	new_user.modify()->password_hash = hash;
	new_user.modify()->permissions   = perms;
}

bool user_db::has_users()
{
	Wt::Dbo::Transaction t{m_dbo};
	return !m_dbo.find<user>().resultList().empty();
}
