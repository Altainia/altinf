#include "post_api_resource.h"

#include "../auth/permission.h"
#include "../auth/session_data.h"
#include "../auth/user_db.h"
#include "../blog/post_writer.h"

#include <Wt/Http/Request.h>
#include <Wt/Http/Response.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Value.h>

#include <ctime>
#include <sstream>
#include <string>

post_api_resource::post_api_resource(std::string db_path, std::filesystem::path posts_dir):
  m_db_path{std::move(db_path)},
  m_posts_dir{std::move(posts_dir)}
{
}

post_api_resource::~post_api_resource()
{
	beingDeleted();
}

static std::string today_string()
{
	const std::time_t now = std::time(nullptr);
	const std::tm*    tm  = std::localtime(&now);
	char              buf[11];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
	return buf;
}

static std::string json_escape(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for(const char c: s)
	{
		switch(c)
		{
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			out += c;
			break;
		}
	}
	return out;
}

static void json_error(Wt::Http::Response& response, int status, const std::string& message)
{
	response.setStatus(status);
	response.setMimeType("application/json");
	response.out() << "{\"status\":\"error\",\"message\":\"" << json_escape(message) << "\"}";
}

void post_api_resource::handleRequest(const Wt::Http::Request& request,
                                      Wt::Http::Response&      response)
{
	if(request.method() != "POST")
	{
		json_error(response, 405, "Method not allowed.");
		return;
	}

	// Parse Bearer token
	const auto auth_header = request.headerValue("Authorization");
	if(auth_header.size() < 8 || auth_header.substr(0, 7) != "Bearer ")
	{
		json_error(response, 401, "Unauthorized.");
		return;
	}
	const auto raw_token = auth_header.substr(7);

	// Authenticate — fresh user_db per request (thread safety)
	user_db      db{m_db_path};
	session_data sess;
	if(!db.verify_api_token(raw_token, sess))
	{
		json_error(response, 403, "Forbidden.");
		return;
	}
	if(!has_permission(sess.permissions, permission::post_write))
	{
		json_error(response, 403, "Forbidden.");
		return;
	}

	// Read request body
	std::ostringstream body_buf;
	body_buf << request.in().rdbuf();
	const auto body_str = body_buf.str();

	// Parse JSON
	Wt::Json::Object     obj;
	Wt::Json::ParseError parse_err;
	if(!Wt::Json::parse(body_str, obj, parse_err))
	{
		json_error(response, 400, parse_err.what());
		return;
	}

	const std::string title = obj.get("title").toString().orIfNull("");
	const std::string date  = obj.get("date").toString().orIfNull("");
	const std::string tags  = obj.get("tags").toString().orIfNull("");
	const std::string body  = obj.get("body").toString().orIfNull("");

	if(title.empty())
	{
		json_error(response, 400, "Title is required.");
		return;
	}

	const auto effective_date = date.empty() ? today_string() : date;

	const auto [filepath, slug] = resolve_new_post(m_posts_dir, effective_date, title);

	if(!write_post_file(filepath, title, effective_date, tags, body))
	{
		json_error(response, 500, "Failed to write file.");
		return;
	}

	response.setStatus(201);
	response.setMimeType("application/json");
	response.out() << "{\"status\":\"ok\",\"slug\":\"" << slug << "\"}";
}
