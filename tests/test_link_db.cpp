#include <Wt/WDate.h>

#include <catch2/catch_test_macros.hpp>

#include "links/link_db.hpp"

static link_entry make_link(const std::string& url,
                            const std::string& title,
                            const std::string& section    = "General",
                            int                sort_order = 0)
{
	link_entry e;
	e.url         = url;
	e.title       = title;
	e.description = "";
	e.section     = section;
	e.sort_order  = sort_order;
	e.added_date  = Wt::WDate{2024, 1, 1};
	return e;
}

TEST_CASE("link_db - starts empty")
{
	link_db db{":memory:"};
	CHECK(db.load_all().empty());
}

TEST_CASE("link_db - add returns positive id")
{
	link_db   db{":memory:"};
	long long id = db.add(make_link("https://example.com", "Example"));
	CHECK(id > 0);
}

TEST_CASE("link_db - find after add")
{
	link_db   db{":memory:"};
	long long id    = db.add(make_link("https://example.com", "Example", "Tech", 5));
	auto      found = db.find(id);
	REQUIRE(found.has_value());
	CHECK(found->url == "https://example.com");
	CHECK(found->title == "Example");
	CHECK(found->section == "Tech");
	CHECK(found->sort_order == 5);
}

TEST_CASE("link_db - find missing id returns nullopt")
{
	link_db db{":memory:"};
	CHECK(!db.find(9999).has_value());
}

TEST_CASE("link_db - update changes stored fields")
{
	link_db    db{":memory:"};
	long long  id = db.add(make_link("https://example.com", "Example"));
	link_entry e;
	e.id          = id;
	e.url         = "https://updated.com";
	e.title       = "Updated";
	e.description = "New desc";
	e.section     = "Work";
	e.sort_order  = 10;
	e.added_date  = Wt::WDate{2025, 3, 1};
	db.update(e);
	auto found = db.find(id);
	REQUIRE(found.has_value());
	CHECK(found->url == "https://updated.com");
	CHECK(found->title == "Updated");
	CHECK(found->description == "New desc");
	CHECK(found->section == "Work");
	CHECK(found->sort_order == 10);
}

TEST_CASE("link_db - update unknown id is a no-op")
{
	link_db    db{":memory:"};
	link_entry e;
	e.id    = 9999;
	e.title = "Ghost";
	db.update(e);
	CHECK(db.load_all().empty());
}

TEST_CASE("link_db - remove deletes the entry")
{
	link_db   db{":memory:"};
	long long id = db.add(make_link("https://example.com", "Example"));
	db.remove(id);
	CHECK(!db.find(id).has_value());
	CHECK(db.load_all().empty());
}

TEST_CASE("link_db - remove unknown id is a no-op")
{
	link_db db{":memory:"};
	db.add(make_link("https://example.com", "Example"));
	db.remove(9999);
	CHECK(db.load_all().size() == 1);
}

TEST_CASE("link_db - load_all sorted by section then sort_order then added_date")
{
	link_db db{":memory:"};
	db.add(make_link("https://b.com", "B", "Work", 1));
	db.add(make_link("https://a.com", "A", "Work", 0));
	db.add(make_link("https://z.com", "Z", "Misc", 0));

	auto all = db.load_all();
	REQUIRE(all.size() == 3);
	CHECK(all[0].section == "Misc");
	CHECK(all[1].section == "Work");
	CHECK(all[1].title == "A");
	CHECK(all[2].section == "Work");
	CHECK(all[2].title == "B");
}

TEST_CASE("link_db - load_all returns all entries")
{
	link_db db{":memory:"};
	db.add(make_link("https://one.com", "One"));
	db.add(make_link("https://two.com", "Two"));
	db.add(make_link("https://three.com", "Three"));
	CHECK(db.load_all().size() == 3);
}
