#include "post_api_resource.h"

#include "../auth/permission.h"
#include "../auth/session_data.h"
#include "../auth/user_db.h"

#include <Wt/Http/Request.h>
#include <Wt/Http/Response.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Value.h>
#include <Wt/Utils.h>

#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
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

static void json_error(Wt::Http::Response& response, int status, const std::string& message)
{
	response.setStatus(status);
	response.setMimeType("application/json");
	response.out() << "{\"status\":\"error\",\"message\":\"" << message << "\"}";
}

void post_api_resource::handleRequest(const Wt::Http::Request& request,
                                      Wt::Http::Response&      response)
{
	if(request.method() != "POST")
	{
		json_error(response, 405, "Method not allowed.");
		return;
	}

	// Parse HTTP Basic Auth
	const auto auth_header = request.headerValue("Authorization");
	if(auth_header.size() < 7 || auth_header.substr(0, 6) != "Basic ")
	{
		json_error(response, 401, "Unauthorized.");
		return;
	}

	const auto decoded = Wt::Utils::base64Decode(auth_header.substr(6));
	const auto colon   = decoded.find(':');
	if(colon == std::string::npos)
	{
		json_error(response, 401, "Unauthorized.");
		return;
	}
	const auto api_user = decoded.substr(0, colon);
	const auto api_pass = decoded.substr(colon + 1);

	// Authenticate — fresh user_db per request (thread safety)
	user_db      db{m_db_path};
	session_data sess;
	if(!db.authenticate(api_user, api_pass, sess))
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

	// Build slug and resolve filename collision
	auto        slug     = make_slug(title);
	std::string base     = effective_date + "-" + slug;
	auto        filepath = m_posts_dir / (base + ".md");

	if(std::filesystem::exists(filepath))
	{
		for(int n = 2;; ++n)
		{
			const auto candidate = m_posts_dir / (base + "-" + std::to_string(n) + ".md");
			if(!std::filesystem::exists(candidate))
			{
				filepath = candidate;
				slug     = slug + "-" + std::to_string(n);
				break;
			}
		}
	}

	// Write post file
	std::ofstream out{filepath};
	if(!out)
	{
		json_error(response, 500, "Failed to write file.");
		return;
	}

	out << "---\n";
	out << "title: " << title << "\n";
	out << "date: " << effective_date << "\n";
	out << "tags: " << tags << "\n";
	out << "---\n";
	out << body;
	out.flush();

	response.setStatus(201);
	response.setMimeType("application/json");
	response.out() << "{\"status\":\"ok\",\"slug\":\"" << slug << "\"}";
}

std::string post_api_resource::make_slug(const std::string& title)
{
	std::string slug;
	for(const char c: title)
	{
		if(std::isalnum(static_cast<unsigned char>(c)))
			slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		else if(std::isspace(static_cast<unsigned char>(c)) && !slug.empty() && slug.back() != '-')
			slug += '-';
	}
	while(!slug.empty() && slug.back() == '-')
		slug.pop_back();
	return slug.empty() ? "post" : slug;
}
