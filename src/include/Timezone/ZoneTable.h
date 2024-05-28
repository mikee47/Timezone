#pragma once

#include <CSV/Table.h>

/**
 * @brief Access a single zone information record
 */
struct Zone : public CSV::Record {
	enum ColumnIndex {
		col_code,
		col_coordinates,
		col_name,
		col_comments,
	};

	using Record::Record;

	CStringArray codes() const
	{
		String s = row[col_code];
		s.replace(',', '\0');
		return CStringArray(std::move(s));
	}

	const char* name() const
	{
		return row[col_name];
	}

	const char* nameNoArea() const
	{
		auto s = name();
		auto sep = strchr(s, '/');
		return sep ? (sep + 1) : s;
	}

	const char* comments() const
	{
		return row[col_comments];
	}

	const char* caption() const
	{
		return comments() ?: nameNoArea();
	}

	String area() const
	{
		auto s = name();
		auto sep = strchr(s, '/');
		return sep ? String(s, sep - s) : nullptr;
	}

	bool areaIs(const String& area) const
	{
		auto s = name();
		auto len = area.length();
		return strncmp(s, area.c_str(), len) == 0 && s[len] == '/';
	}

	String getAreaCaption() const
	{
		return getAreaCaption(area());
	}

	static String getAreaCaption(const String& name);
};

/**
 * @brief Access zone table entries stored in CSV format
 */
class ZoneTable : public CSV::Table<Zone>
{
public:
	ZoneTable(const String& filename);
};

/**
 * @brief Class to assist with filtering zone table entries
 */
class ZoneFilter
{
public:
	ZoneFilter(ZoneTable& table, bool sorted = false) : table(table), sorted(sorted)
	{
	}

	/*
	* @brief Find all matching timezone entries
	* @param filter String to match against
	* @param includePathSep true to include any trailing '/' in returned values, otherwise this is omitted
	*
	* The matching algorithm avoids returning excessive numbers of entries as this is generally unhelpful anyway.
	* Matching is best illustrated by some examples:
	*
	*   filter				root				matches
	*   ------------------  --------------  	------------------------------
	*   ""					""					"Africa/", "America/", ...
	*  	"eur"				""					"Europe/"
	*  	"europe/"			"Europe/"			"London", "Berlin", ...
	*  	"america/ind" 		"America/"			"Indiana/"
	*  	"America/Indiana/" 	"America/Indiana/"	"Tell_City", "Knox", ...
	*
	*/
	size_t match(const String& filter, bool includePathSep);

	void sortMatches();

	String operator[](unsigned index) const
	{
		return root + matches[index];
	}

	/**
	 * @brief Initial part common to all matches
	 */
	String root;

	/**
	 * @brief List of matched values
	 */
	Vector<String> matches;

private:
	ZoneTable& table;
	bool sorted;
};
