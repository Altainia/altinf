#include "user_db.hpp"

#include <Wt/Auth/HashFunction.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <stdexcept>

#include "api_token.hpp"
#include "session_token.hpp"

static std::string sha256_hex(const std::string& input)
{
	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int  len = 0;
	EVP_Digest(input.data(), input.size(), hash, &len, EVP_sha256(), nullptr);

	static constexpr char hex[] = "0123456789abcdef";
	std::string           out;
	out.reserve(len * 2);
	for(unsigned int i = 0; i < len; ++i)
	{
		out += hex[(hash[i] >> 4) & 0xf];
		out += hex[hash[i] & 0xf];
	}
	return out;
}

static std::string generate_raw_token()
{
	std::array<unsigned char, 32> bytes{};
	if(RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
	{
		throw std::runtime_error{"RAND_bytes failed"};
	}

	static constexpr char hex[] = "0123456789abcdef";
	std::string           out;
	out.reserve(64);
	for(auto b: bytes)
	{
		out += hex[(b >> 4) & 0xf];
		out += hex[b & 0xf];
	}
	return out;
}

user_db::user_db(const std::string& db_path)
{
	m_dbo.setConnection(std::make_unique<Wt::Dbo::backend::Sqlite3>(db_path));
	m_dbo.mapClass<user>("user");
	m_dbo.mapClass<api_token>("api_token");
	m_dbo.mapClass<session_token>("session_token");

	try
	{
		m_dbo.createTables();
	}
	catch(const Wt::Dbo::Exception&)
	{
		// Tables already exist — run targeted migrations for any new columns/tables.
		Wt::Dbo::Transaction t{m_dbo};
		m_dbo.execute(
		  "CREATE TABLE IF NOT EXISTS \"api_token\" ("
		  "  \"id\" integer primary key autoincrement,"
		  "  \"version\" integer not null,"
		  "  \"token_hash\" text not null,"
		  "  \"username\" text not null"
		  ")");
		m_dbo.execute(
		  "CREATE TABLE IF NOT EXISTS \"session_token\" ("
		  "  \"id\" integer primary key autoincrement,"
		  "  \"version\" integer not null,"
		  "  \"token_hash\" text not null,"
		  "  \"username\" text not null"
		  ")");
		try
		{
			m_dbo.execute("ALTER TABLE \"user\" ADD COLUMN \"display_name\" text not null default ''");
		}
		catch(const Wt::Dbo::Exception&)
		{
			// Column already exists — nothing to do.
		}
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
	{
		return false;
	}

	const Wt::Dbo::ptr<user> found = *results.begin();

	const Wt::Auth::BCryptHashFunction bcrypt{12};
	if(!bcrypt.verify(password, "", found->password_hash))
	{
		return false;
	}

	out.logged_in    = true;
	out.username     = uname;
	out.display_name = found->display_name;
	out.permissions  = found->permissions;

	return true;
}

void user_db::create_user(const std::string& uname,
                          const std::string& password,
                          permission::flags  perms,
                          const std::string& display_name)
{
	const Wt::Auth::BCryptHashFunction bcrypt{12};
	const std::string                  hash = bcrypt.compute(password, "");

	Wt::Dbo::Transaction t{m_dbo};
	auto                 new_user    = m_dbo.add(std::make_unique<user>());
	new_user.modify()->username      = uname;
	new_user.modify()->display_name  = display_name;
	new_user.modify()->password_hash = hash;
	new_user.modify()->permissions   = perms;
}

bool user_db::has_users()
{
	Wt::Dbo::Transaction t{m_dbo};
	return !m_dbo.find<user>().resultList().empty();
}

std::vector<user_entry> user_db::list_users()
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results = m_dbo.find<user>().resultList();

	std::vector<user_entry> out;
	for(const auto& u: results)
	{
		user_entry e;
		e.username     = u->username;
		e.display_name = u->display_name;
		e.permissions  = u->permissions;
		out.push_back(std::move(e));
	}
	return out;
}

bool user_db::username_exists(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	return !m_dbo.find<user>().where("username = ?").bind(username).resultList().empty();
}

void user_db::delete_user(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	// Remove API tokens first
	const auto tokens =
	  m_dbo.find<api_token>().where("username = ?").bind(username).resultList();
	for(const auto& tok: tokens)
	{
		Wt::Dbo::ptr<api_token> p = tok;
		p.remove();
	}

	// Remove session tokens
	const auto session_toks =
	  m_dbo.find<session_token>().where("username = ?").bind(username).resultList();
	for(const auto& tok: session_toks)
	{
		Wt::Dbo::ptr<session_token> p = tok;
		p.remove();
	}

	const auto results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	for(const auto& u: results)
	{
		Wt::Dbo::ptr<user> p = u;
		p.remove();
	}
}

void user_db::update_user(const std::string& username,
                          const std::string& display_name,
                          permission::flags  perms)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return;
	}

	const auto u             = *results.begin();
	u.modify()->display_name = display_name;
	u.modify()->permissions  = perms;
}

void user_db::set_password(const std::string& username, const std::string& new_password)
{
	const Wt::Auth::BCryptHashFunction bcrypt{12};
	const std::string                  hash = bcrypt.compute(new_password, "");

	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return;
	}

	const auto u              = *results.begin();
	u.modify()->password_hash = hash;
}

std::vector<api_token_entry> user_db::list_tokens(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<api_token>().where("username = ?").bind(username).resultList();

	std::vector<api_token_entry> out;
	for(const auto& tok: results)
	{
		api_token_entry e;
		e.id         = tok.id();
		e.token_hash = tok->token_hash;
		out.push_back(std::move(e));
	}
	return out;
}

void user_db::delete_token(long long token_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<api_token>().where("id = ?").bind(token_id).resultList();
	for(const auto& tok: results)
	{
		Wt::Dbo::ptr<api_token> p = tok;
		p.remove();
	}
}

std::string user_db::create_api_token(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(users.empty())
	{
		throw std::runtime_error{"Unknown user: " + username};
	}

	const auto raw_token = generate_raw_token();
	const auto hash      = sha256_hex(raw_token);

	auto tok                 = m_dbo.add(std::make_unique<api_token>());
	tok.modify()->token_hash = hash;
	tok.modify()->username   = username;

	return raw_token;
}

bool user_db::verify_api_token(const std::string& raw_token, session_data& out)
{
	const auto hash = sha256_hex(raw_token);

	Wt::Dbo::Transaction t{m_dbo};

	const auto tokens =
	  m_dbo.find<api_token>().where("token_hash = ?").bind(hash).resultList();
	if(tokens.empty())
	{
		return false;
	}

	const auto tok_username = (*tokens.begin())->username;

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(tok_username).resultList();
	if(users.empty())
	{
		return false;
	}

	const auto found = *users.begin();
	out.logged_in    = true;
	out.username     = tok_username;
	out.display_name = found->display_name;
	out.permissions  = found->permissions;

	return true;
}

std::string user_db::create_session_token(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(users.empty())
	{
		throw std::runtime_error{"Unknown user: " + username};
	}

	const auto raw_token = generate_raw_token();
	const auto hash      = sha256_hex(raw_token);

	auto tok                 = m_dbo.add(std::make_unique<session_token>());
	tok.modify()->token_hash = hash;
	tok.modify()->username   = username;

	return raw_token;
}

bool user_db::verify_session_token(const std::string& raw_token, session_data& out)
{
	const auto hash = sha256_hex(raw_token);

	Wt::Dbo::Transaction t{m_dbo};

	const auto tokens =
	  m_dbo.find<session_token>().where("token_hash = ?").bind(hash).resultList();
	if(tokens.empty())
	{
		return false;
	}

	const auto tok_username = (*tokens.begin())->username;

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(tok_username).resultList();
	if(users.empty())
	{
		return false;
	}

	const auto found = *users.begin();
	out.logged_in    = true;
	out.username     = tok_username;
	out.display_name = found->display_name;
	out.permissions  = found->permissions;

	return true;
}

void user_db::delete_session_token(const std::string& raw_token)
{
	const auto hash = sha256_hex(raw_token);

	Wt::Dbo::Transaction t{m_dbo};

	const auto tokens =
	  m_dbo.find<session_token>().where("token_hash = ?").bind(hash).resultList();
	for(const auto& tok: tokens)
	{
		Wt::Dbo::ptr<session_token> p = tok;
		p.remove();
	}
}
