#include "user_db.h"

#include "api_token.h"

#include <Wt/Auth/HashFunction.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <stdexcept>

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
		throw std::runtime_error{"RAND_bytes failed"};

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
	out.permissions = static_cast<uint64_t>(found->permissions);

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
	new_user.modify()->permissions   = static_cast<long long>(perms);
}

bool user_db::has_users()
{
	Wt::Dbo::Transaction t{m_dbo};
	return !m_dbo.find<user>().resultList().empty();
}

std::string user_db::create_api_token(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(users.empty())
		throw std::runtime_error{"Unknown user: " + username};

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
		return false;

	const auto tok_username = (*tokens.begin())->username;

	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(tok_username).resultList();
	if(users.empty())
		return false;

	out.logged_in   = true;
	out.username    = tok_username;
	out.permissions = static_cast<uint64_t>((*users.begin())->permissions);

	return true;
}
