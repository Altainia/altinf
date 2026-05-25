#include <catch2/catch_test_macros.hpp>

#include "api/post_api_json.hpp"

TEST_CASE("api::json_error_body - well-formed JSON")
{
	CHECK(api::json_error_body("Method not allowed.") ==
	      R"({"status":"error","message":"Method not allowed."})");
}

TEST_CASE("api::json_error_body - escapes double quotes in message")
{
	CHECK(api::json_error_body(R"(say "hello")") ==
	      R"({"status":"error","message":"say \"hello\""})");
}

TEST_CASE("api::json_error_body - escapes backslash in message")
{
	CHECK(api::json_error_body("C:\\path") ==
	      R"({"status":"error","message":"C:\\path"})");
}

TEST_CASE("api::json_ok_body - well-formed JSON")
{
	CHECK(api::json_ok_body("my-post-slug") ==
	      R"({"status":"ok","slug":"my-post-slug"})");
}
