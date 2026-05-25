#pragma once

#include <format>
#include <string>

namespace api
{
	inline std::string json_escape(const std::string& s)
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

	inline std::string json_error_body(const std::string& message)
	{
		return std::format(R"---({{"status":"error","message":"{}"}})---", json_escape(message));
	}

	inline std::string json_ok_body(const std::string& slug)
	{
		return std::format(R"---({{"status":"ok","slug":"{}"}})---", slug);
	}
}
