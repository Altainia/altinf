#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "blog/post_writer.hpp"

namespace
{
	// A unique temp directory removed at scope exit.
	struct temp_dir
	{
		std::filesystem::path path;

		temp_dir():
		  path{std::filesystem::temp_directory_path() /
		       ("altinf_post_writer_" + std::to_string(::getpid()) + "_" +
		        std::to_string(reinterpret_cast<std::uintptr_t>(this)))}
		{
			std::filesystem::create_directories(path);
		}

		~temp_dir()
		{
			std::error_code ec;
			std::filesystem::remove_all(path, ec);
		}
	};

	std::string read_all(const std::filesystem::path& p)
	{
		std::ifstream     in{p};
		std::stringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}
}

TEST_CASE("write_post_file creates the parent directory if it is missing")
{
	const temp_dir tmp;
	// posts_dir does not exist yet — mirrors a fresh /data volume in production.
	const auto posts_dir = tmp.path / "posts";
	REQUIRE_FALSE(std::filesystem::exists(posts_dir));

	const auto filepath = posts_dir / "2026-06-01-hello.md";

	const bool ok = write_post_file(
	  filepath, "Hello", Wt::WDate{2026, 6, 1}, std::nullopt, "tag1, tag2", "Body text\n");

	CHECK(ok);
	CHECK(std::filesystem::exists(filepath));

	const auto contents = read_all(filepath);
	CHECK(contents.find("title: Hello") != std::string::npos);
	CHECK(contents.find("Body text") != std::string::npos);
}

TEST_CASE("write_post_file writes into an existing directory")
{
	const temp_dir tmp;
	const auto     filepath = tmp.path / "2026-06-01-hi.md";

	const bool ok =
	  write_post_file(filepath, "Hi", Wt::WDate{2026, 6, 1}, std::nullopt, "", "x");

	CHECK(ok);
	CHECK(std::filesystem::exists(filepath));
}
