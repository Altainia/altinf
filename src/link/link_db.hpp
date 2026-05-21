#pragma once

#include "link.hpp"

#include <Wt/Dbo/Session.h>

#include <optional>
#include <string>
#include <vector>

class link_db
{
public:
	explicit link_db(const std::string& db_path);

	std::vector<link_entry>   load_all();
	std::optional<link_entry> find(long long id);
	long long                 add(const link_entry& e);
	void                      update(const link_entry& e);
	void                      remove(long long id);

private:
	Wt::Dbo::Session m_dbo;
};
