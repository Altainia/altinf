#include "user_db.hpp"

#include <Wt/Auth/HashFunction.h>
#include <Wt/Dbo/Transaction.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <Wt/WDateTime.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <stdexcept>

#include "api_token.hpp"
#include "db/migrator.hpp"
#include "google_identity.hpp"
#include "session_token.hpp"

namespace
{
	// Post-baseline schema deltas for the "auth" domain, in version order. The
	// baseline (version 1) is the schema createTables() builds from the user,
	// api_token and session_token structs.
	const std::vector<db::migrator::migration>& auth_migrations()
	{
		static const std::vector<db::migrator::migration> migrations{
		  // v2: link Google accounts to existing users. DDL mirrors what
		  // createTables() generates for the google_identity struct so fresh and
		  // migrated databases converge.
		  db::migrator::sql_migration(
		    2,
		    "add google_identity table",
		    "CREATE TABLE \"google_identity\" ("
		    "\"id\" integer primary key autoincrement, "
		    "\"version\" integer not null, "
		    "\"username\" text not null, "
		    "\"google_sub\" text not null, "
		    "\"email\" text not null)"),
		  // v3: user audit trail. Two tables created together; DDL mirrors what
		  // createTables() generates for user_event_record / user_field_change_record.
		  db::migrator::migration{
		    3,
		    "add user audit tables",
		    [](Wt::Dbo::Session& session) {
			    session.execute(
			      "CREATE TABLE \"user_event\" ("
			      "\"id\" integer primary key autoincrement, "
			      "\"version\" integer not null, "
			      "\"user_id\" bigint not null, "
			      "\"actor_id\" bigint not null, "
			      "\"occurred_at\" text not null, "
			      "\"event_type\" text not null)");
			    session.execute(
			      "CREATE TABLE \"user_field_change\" ("
			      "\"id\" integer primary key autoincrement, "
			      "\"version\" integer not null, "
			      "\"event_id\" bigint not null, "
			      "\"field_name\" text not null, "
			      "\"old_value\" text not null, "
			      "\"new_value\" text not null)");
		    }},
		  // v4: give API tokens a human-readable name. Existing tokens are
		  // backfilled with a stable placeholder derived from their id.
		  db::migrator::migration{
		    4,
		    "add api_token.name column",
		    [](Wt::Dbo::Session& session) {
			    session.execute(
			      "ALTER TABLE \"api_token\" ADD COLUMN \"name\" text not null default ''");
			    session.execute(
			      "UPDATE \"api_token\" SET \"name\" = 'token-' || id WHERE \"name\" = ''");
		    }},
		  // v5: soft-delete support. Existing users are active (empty deleted_at).
		  db::migrator::sql_migration(
		    5,
		    "add user.deleted_at column",
		    "ALTER TABLE \"user\" ADD COLUMN \"deleted_at\" text not null default ''"),
		  // v6: link auth child tables to the user by id. Add user_id and backfill
		  // from the username; usernames with no surviving user row (orphans from
		  // pre-soft-delete hard deletes) map to 0 ("unknown"). Username columns are
		  // retained denormalized during the transition.
		  db::migrator::migration{
		    6,
		    "add user_id to api_token/session_token/google_identity",
		    [](Wt::Dbo::Session& session) {
			    for(const char* table: {"api_token", "session_token", "google_identity"})
			    {
				    session.execute(std::string{"ALTER TABLE \""} + table +
				                    "\" ADD COLUMN \"user_id\" bigint not null default 0");
				    session.execute(
				      std::string{"UPDATE \""} + table +
				      "\" SET \"user_id\" = COALESCE("
				      "(SELECT id FROM \"user\" WHERE \"user\".username = \"" +
				      table + "\".username), 0)");
			    }
		    }},
		  // v7: contract phase — user_id is now the sole link, drop the
		  // denormalized username columns.
		  db::migrator::migration{
		    7,
		    "drop username from api_token/session_token/google_identity",
		    [](Wt::Dbo::Session& session) {
			    for(const char* table: {"api_token", "session_token", "google_identity"})
			    {
				    session.execute(std::string{"ALTER TABLE \""} + table +
				                    "\" DROP COLUMN \"username\"");
			    }
		    }},
		};
		return migrations;
	}
} // namespace

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
	m_dbo.mapClass<google_identity>("google_identity");
	m_dbo.mapClass<user_event_record>("user_event");
	m_dbo.mapClass<user_field_change_record>("user_field_change");

	db::migrator::run(m_dbo, "auth", auth_migrations());
}

bool user_db::authenticate(const std::string& uname,
                           const std::string& password,
                           session_data&      out)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results = m_dbo.find<user>()
	                       .where("username = ? and deleted_at = ''")
	                       .bind(uname)
	                       .resultList();

	if(results.empty())
	{
		return false;
	}

	const Wt::Dbo::ptr<user> found = *results.begin();

	const Wt::Auth::BCryptHashFunction bcrypt{12};
	if(found->password_hash.empty() || !bcrypt.verify(password, "", found->password_hash))
	{
		return false;
	}

	out.logged_in    = true;
	out.user_id      = found.id();
	out.username     = uname;
	out.display_name = found->display_name;
	out.permissions  = found->permissions;

	return true;
}

bool user_db::load_session(const std::string& uname, session_data& out)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<user>().where("username = ? and deleted_at = ''").bind(uname).resultList();
	if(results.empty())
	{
		return false;
	}

	const Wt::Dbo::ptr<user> found = *results.begin();

	out.logged_in    = true;
	out.user_id      = found.id();
	out.username     = uname;
	out.display_name = found->display_name;
	out.permissions  = found->permissions;

	return true;
}

void user_db::create_user(const std::string& uname,
                          const std::string& password,
                          permission::flags  perms,
                          const std::string& display_name,
                          long long          actor_id)
{
	const Wt::Auth::BCryptHashFunction bcrypt{12};
	const std::string                  hash = bcrypt.compute(password, "");

	Wt::Dbo::Transaction t{m_dbo};
	auto                 new_user    = m_dbo.add(std::make_unique<user>());
	new_user.modify()->username      = uname;
	new_user.modify()->display_name  = display_name;
	new_user.modify()->password_hash = hash;
	new_user.modify()->permissions   = perms;
	m_dbo.flush();

	record_user_event(new_user.id(), actor_id, "created");
}

bool user_db::has_users()
{
	Wt::Dbo::Transaction t{m_dbo};
	return !m_dbo.find<user>().resultList().empty();
}

std::vector<user_entry> user_db::list_users(bool include_deleted)
{
	Wt::Dbo::Transaction t{m_dbo};

	auto query = m_dbo.find<user>();
	if(!include_deleted)
	{
		query.where("deleted_at = ''");
	}
	const auto results = query.resultList();

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

bool user_db::has_password(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return false;
	}
	const Wt::Dbo::ptr<user> u = *results.begin();
	return !u->password_hash.empty();
}

bool user_db::verify_password(const std::string& username, const std::string& password)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return false;
	}
	const Wt::Dbo::ptr<user> u = *results.begin();
	if(u->password_hash.empty())
	{
		return false;
	}
	const Wt::Auth::BCryptHashFunction bcrypt{12};
	return bcrypt.verify(password, "", u->password_hash);
}

bool user_db::username_exists(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	return !m_dbo.find<user>().where("username = ?").bind(username).resultList().empty();
}

std::optional<long long> user_db::user_id_for(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};
	return user_id_for_locked(username);
}

std::string user_db::display_name_for_id(long long user_id)
{
	if(user_id == 0)
	{
		return "system";
	}
	Wt::Dbo::Transaction t{m_dbo};
	try
	{
		const Wt::Dbo::ptr<user> u = m_dbo.load<user>(user_id);
		return u->display_name.empty() ? u->username : u->display_name;
	}
	catch(const Wt::Dbo::ObjectNotFoundException&)
	{
		return "(unknown)";
	}
}

std::optional<long long> user_db::user_id_for_locked(const std::string& username)
{
	const auto results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	return results.begin()->id();
}

std::string user_db::username_for_id_locked(long long user_id)
{
	if(user_id == 0)
	{
		return {};
	}
	try
	{
		return m_dbo.load<user>(user_id)->username;
	}
	catch(const Wt::Dbo::ObjectNotFoundException&)
	{
		return {};
	}
}

void user_db::clear_google_link_locked(long long user_id)
{
	if(user_id == 0)
	{
		return;
	}
	const auto google_links =
	  m_dbo.find<google_identity>().where("user_id = ?").bind(user_id).resultList();
	for(const auto& g: google_links)
	{
		Wt::Dbo::ptr<google_identity> p = g;
		p.remove();
	}
}

void user_db::delete_user(const std::string& username, long long actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<user>().where("username = ? and deleted_at = ''").bind(username).resultList();
	if(results.empty())
	{
		return;
	}
	const auto      u   = *results.begin();
	const long long uid = u.id();

	// Revoke every credential so a deleted account cannot authenticate.
	const auto tokens =
	  m_dbo.find<api_token>().where("user_id = ?").bind(uid).resultList();
	for(const auto& tok: tokens)
	{
		Wt::Dbo::ptr<api_token> p = tok;
		p.remove();
	}

	const auto session_toks =
	  m_dbo.find<session_token>().where("user_id = ?").bind(uid).resultList();
	for(const auto& tok: session_toks)
	{
		Wt::Dbo::ptr<session_token> p = tok;
		p.remove();
	}

	clear_google_link_locked(uid);

	// Soft-delete: retain the row so id->display_name keeps resolving for history.
	u.modify()->deleted_at =
	  Wt::WDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toUTF8();

	record_user_event(u.id(), actor_id, "deleted");
}

void user_db::update_user(const std::string& username,
                          const std::string& display_name,
                          permission::flags  perms,
                          long long          actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return;
	}

	const auto u             = *results.begin();
	const auto old_name      = u->display_name;
	const auto old_perms     = u->permissions;
	u.modify()->display_name = display_name;
	u.modify()->permissions  = perms;

	if(old_name != display_name)
	{
		record_user_event(u.id(), actor_id, "display_name_changed", {{.field_name = "display_name", .old_value = old_name, .new_value = display_name}});
	}
	if(old_perms != perms)
	{
		record_user_event(u.id(), actor_id, "permissions_changed", {{.field_name = "permissions", .old_value = permission::label(old_perms), .new_value = permission::label(perms)}});
	}
}

void user_db::set_display_name(const std::string& username,
                               const std::string& display_name,
                               long long          actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return;
	}

	const auto u        = *results.begin();
	const auto old_name = u->display_name;
	if(old_name == display_name)
	{
		return;
	}
	u.modify()->display_name = display_name;

	record_user_event(u.id(), actor_id, "display_name_changed", {{.field_name = "display_name", .old_value = old_name, .new_value = display_name}});
}

void user_db::set_password(const std::string& username,
                           const std::string& new_password,
                           long long          actor_id)
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
	const bool had_password   = !u->password_hash.empty();
	u.modify()->password_hash = hash;

	record_user_event(u.id(), actor_id, had_password ? "password_changed" : "password_set");
}

bool user_db::unset_password(const std::string& username, long long actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(results.empty())
	{
		return false;
	}
	const auto u = *results.begin();

	// Refuse to remove the last sign-in method: a Google link must remain.
	const auto google =
	  m_dbo.find<google_identity>().where("user_id = ?").bind(u.id()).resultList();
	if(google.empty())
	{
		return false;
	}

	if(u->password_hash.empty())
	{
		return false; // nothing to unset
	}
	u.modify()->password_hash = "";

	record_user_event(u.id(), actor_id, "password_unset");
	return true;
}

std::vector<api_token_entry> user_db::list_tokens(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto uid = user_id_for_locked(username);
	if(!uid)
	{
		return {};
	}
	const auto results =
	  m_dbo.find<api_token>().where("user_id = ?").bind(*uid).resultList();

	std::vector<api_token_entry> out;
	for(const auto& tok: results)
	{
		api_token_entry e;
		e.id         = tok.id();
		e.token_hash = tok->token_hash;
		e.name       = tok->name;
		out.push_back(std::move(e));
	}
	return out;
}

void user_db::delete_token(long long token_id, long long actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<api_token>().where("id = ?").bind(token_id).resultList();
	for(const auto& tok: results)
	{
		const auto              owner_id = tok->user_id;
		const auto              name     = tok->name;
		Wt::Dbo::ptr<api_token> p        = tok;
		p.remove();
		if(owner_id != 0)
		{
			record_user_event(owner_id, actor_id, "token_revoked", {{.field_name = "token", .old_value = name, .new_value = ""}});
		}
	}
}

std::string user_db::create_api_token(const std::string& username,
                                      const std::string& name,
                                      long long          actor_id)
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

	const auto owner_id      = users.begin()->id();
	auto       tok           = m_dbo.add(std::make_unique<api_token>());
	tok.modify()->token_hash = hash;
	tok.modify()->user_id    = owner_id;
	tok.modify()->name       = name;

	record_user_event(owner_id, actor_id, "token_created", {{.field_name = "token", .old_value = "", .new_value = name}});

	return raw_token;
}

void user_db::rename_api_token(long long token_id, const std::string& new_name, long long actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<api_token>().where("id = ?").bind(token_id).resultList();
	if(results.empty())
	{
		return;
	}

	const auto tok      = *results.begin();
	const auto old_name = tok->name;
	const auto owner_id = tok->user_id;
	tok.modify()->name  = new_name;

	if(owner_id != 0)
	{
		record_user_event(owner_id, actor_id, "token_renamed", {{.field_name = "token", .old_value = old_name, .new_value = new_name}});
	}
}

bool user_db::fill_session_by_id(long long user_id, session_data& out)
{
	if(user_id == 0)
	{
		return false;
	}
	try
	{
		const Wt::Dbo::ptr<user> u = m_dbo.load<user>(user_id);
		if(!u->deleted_at.empty())
		{
			return false;
		}
		out.logged_in    = true;
		out.user_id      = u.id();
		out.username     = u->username;
		out.display_name = u->display_name;
		out.permissions  = u->permissions;
		return true;
	}
	catch(const Wt::Dbo::ObjectNotFoundException&)
	{
		return false;
	}
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
	return fill_session_by_id((*tokens.begin())->user_id, out);
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
	tok.modify()->user_id    = users.begin()->id();

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
	return fill_session_by_id((*tokens.begin())->user_id, out);
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

void user_db::link_google(const std::string& username,
                          const std::string& google_sub,
                          const std::string& email,
                          long long          actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto uid = user_id_for_locked(username);

	// One link per user: drop any existing row before adding the new one.
	clear_google_link_locked(uid.value_or(0));

	auto link                 = m_dbo.add(std::make_unique<google_identity>());
	link.modify()->user_id    = uid.value_or(0);
	link.modify()->google_sub = google_sub;
	link.modify()->email      = email;

	if(uid)
	{
		record_user_event(*uid, actor_id, "google_linked", {{.field_name = "google", .old_value = "", .new_value = email}});
	}
}

bool user_db::unlink_google(const std::string& username, long long actor_id)
{
	Wt::Dbo::Transaction t{m_dbo};

	// Refuse to remove the last sign-in method: a password must remain.
	const auto users =
	  m_dbo.find<user>().where("username = ?").bind(username).resultList();
	if(users.empty())
	{
		return false;
	}
	const Wt::Dbo::ptr<user> u = *users.begin();

	const auto links =
	  m_dbo.find<google_identity>().where("user_id = ?").bind(u.id()).resultList();
	if(links.empty())
	{
		return false; // nothing linked
	}
	const std::string email = (*links.begin())->email;

	if(u->password_hash.empty())
	{
		return false;
	}

	clear_google_link_locked(u.id());

	record_user_event(u.id(), actor_id, "google_unlinked", {{.field_name = "google", .old_value = email, .new_value = ""}});
	return true;
}

std::optional<std::string> user_db::google_email_for(const std::string& username)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto uid = user_id_for_locked(username);
	if(!uid)
	{
		return std::nullopt;
	}
	const auto results =
	  m_dbo.find<google_identity>().where("user_id = ?").bind(*uid).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	return (*results.begin())->email;
}

std::optional<std::string>
  user_db::username_for_google_sub(const std::string& google_sub)
{
	Wt::Dbo::Transaction t{m_dbo};

	const auto results =
	  m_dbo.find<google_identity>().where("google_sub = ?").bind(google_sub).resultList();
	if(results.empty())
	{
		return std::nullopt;
	}
	const auto username = username_for_id_locked((*results.begin())->user_id);
	if(username.empty())
	{
		return std::nullopt;
	}
	return username;
}

// ── Audit trail ──────────────────────────────────────────────────────────────

void user_db::record_user_event(long long                                   user_id,
                                long long                                   actor_id,
                                const std::string&                          event_type,
                                const std::vector<user_field_change_entry>& changes)
{
	const std::string ts =
	  Wt::WDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toUTF8();

	auto ev                  = m_dbo.add(std::make_unique<user_event_record>());
	ev.modify()->user_id     = user_id;
	ev.modify()->actor_id    = actor_id;
	ev.modify()->occurred_at = ts;
	ev.modify()->event_type  = event_type;
	m_dbo.flush();

	for(const auto& ch: changes)
	{
		auto fc                 = m_dbo.add(std::make_unique<user_field_change_record>());
		fc.modify()->event_id   = ev.id();
		fc.modify()->field_name = ch.field_name;
		fc.modify()->old_value  = ch.old_value;
		fc.modify()->new_value  = ch.new_value;
	}
}

std::vector<user_event_entry> user_db::history_for_user(long long user_id)
{
	Wt::Dbo::Transaction t{m_dbo};
	const auto           events = m_dbo.find<user_event_record>()
	                      .where("user_id = ?")
	                      .bind(user_id)
	                      .orderBy("id DESC")
	                      .resultList();
	std::vector<user_event_entry> out;
	for(const auto& ev: events)
	{
		user_event_entry entry;
		entry.id          = ev.id();
		entry.user_id     = ev->user_id;
		entry.actor_id    = ev->actor_id;
		entry.occurred_at = ev->occurred_at;
		entry.event_type  = ev->event_type;

		const auto changes = m_dbo.find<user_field_change_record>()
		                       .where("event_id = ?")
		                       .bind(ev.id())
		                       .resultList();
		for(const auto& ch: changes)
		{
			entry.changes.push_back({.field_name = ch->field_name,
			                         .old_value  = ch->old_value,
			                         .new_value  = ch->new_value});
		}
		out.push_back(std::move(entry));
	}
	return out;
}