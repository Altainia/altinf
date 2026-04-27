#include <Wt/WDate.h>

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "blog/blog_loader.hpp"

static std::filesystem::path make_temp_dir()
{
	std::string tmpl = (std::filesystem::temp_directory_path() / "altinf_blog_XXXXXX").string();
	if(mkdtemp(tmpl.data()) == nullptr)
	{
		throw std::runtime_error{"mkdtemp failed"};
	}
	return tmpl;
}

static void write_post(const std::filesystem::path& dir,
                       const std::string&           filename,
                       const std::string&           content)
{
	std::ofstream f{dir / filename};
	f << content;
}

TEST_CASE("blog_loader - missing directory returns empty")
{
	blog_loader loader{"/nonexistent/path/that/cannot/exist"};
	CHECK(loader.load().empty());
}

TEST_CASE("blog_loader - empty directory returns empty")
{
	auto        tmp = make_temp_dir();
	blog_loader loader{tmp};
	CHECK(loader.load().empty());
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - non-md files are ignored")
{
	auto tmp = make_temp_dir();
	write_post(tmp, "notes.txt", "---\ntitle: Ignored\ndate: 2024-01-01\n---\n");
	write_post(tmp, "2024-01-01-real.md", "---\ntitle: Real\ndate: 2024-01-01\n---\n");
	blog_loader loader{tmp};
	auto        posts = loader.load();
	REQUIRE(posts.size() == 1);
	CHECK(posts[0].slug == "real");
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - parses title, date, and tags")
{
	auto tmp = make_temp_dir();
	write_post(tmp,
	           "2024-03-10-hello-world.md",
	           "---\ntitle: Hello World\ndate: 2024-03-10\ntags: c++, testing\n---\n## Body\n");
	blog_loader loader{tmp};
	auto        posts = loader.load();
	REQUIRE(posts.size() == 1);
	CHECK(posts[0].title == "Hello World");
	CHECK(posts[0].date == Wt::WDate{2024, 3, 10});
	CHECK(!posts[0].last_modified.has_value());
	CHECK(posts[0].slug == "hello-world");
	REQUIRE(posts[0].tags.size() == 2);
	CHECK(posts[0].tags[0] == "c++");
	CHECK(posts[0].tags[1] == "testing");
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - parses last_modified")
{
	auto tmp = make_temp_dir();
	write_post(tmp,
	           "2024-01-15-post.md",
	           "---\ntitle: Post\ndate: 2024-01-15\nlast_modified: 2024-06-01\n---\n");
	blog_loader loader{tmp};
	auto        posts = loader.load();
	REQUIRE(posts.size() == 1);
	REQUIRE(posts[0].last_modified.has_value());
	CHECK(*posts[0].last_modified == Wt::WDate{2024, 6, 1});
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - slug strips leading date prefix")
{
	auto tmp = make_temp_dir();
	write_post(tmp, "2024-07-04-independence-day.md", "---\ntitle: T\ndate: 2024-07-04\n---\n");
	blog_loader loader{tmp};
	CHECK(loader.load()[0].slug == "independence-day");
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - slug preserved when no date prefix")
{
	auto tmp = make_temp_dir();
	write_post(tmp, "about.md", "---\ntitle: About\ndate: 2024-01-01\n---\n");
	blog_loader loader{tmp};
	CHECK(loader.load()[0].slug == "about");
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - missing frontmatter delimiter leaves post empty")
{
	auto tmp = make_temp_dir();
	write_post(tmp, "2024-01-01-broken.md", "No frontmatter here\n");
	blog_loader loader{tmp};
	auto        posts = loader.load();
	REQUIRE(posts.size() == 1);
	CHECK(posts[0].title.empty());
	CHECK(!posts[0].date.isValid());
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - posts sorted descending by date")
{
	auto tmp = make_temp_dir();
	write_post(tmp, "2024-01-01-alpha.md", "---\ntitle: Alpha\ndate: 2024-01-01\n---\n");
	write_post(tmp, "2024-06-15-beta.md", "---\ntitle: Beta\ndate: 2024-06-15\n---\n");
	write_post(tmp, "2023-12-31-gamma.md", "---\ntitle: Gamma\ndate: 2023-12-31\n---\n");
	blog_loader loader{tmp};
	auto        posts = loader.load();
	REQUIRE(posts.size() == 3);
	CHECK(posts[0].slug == "beta");
	CHECK(posts[1].slug == "alpha");
	CHECK(posts[2].slug == "gamma");
	std::filesystem::remove_all(tmp);
}

TEST_CASE("blog_loader - whitespace trimmed from frontmatter values")
{
	auto tmp = make_temp_dir();
	write_post(tmp, "2024-01-01-spaced.md", "---\ntitle:   Spaced Title   \ndate: 2024-01-01\n---\n");
	blog_loader loader{tmp};
	CHECK(loader.load()[0].title == "Spaced Title");
	std::filesystem::remove_all(tmp);
}
