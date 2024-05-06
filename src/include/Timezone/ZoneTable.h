#pragma once

#include <Data/CsvReader.h>
#include <memory>

/**
 * @brief Access a single zone information record
 */
class Zone
{
public:
	Zone()
	{
	}

	Zone(const CStringArray& row) : row(&row)
	{
	}

	explicit operator bool() const
	{
		return row;
	}

	CStringArray codes() const
	{
		String s = getCell(0);
		s.replace(',', '\0');
		return CStringArray(std::move(s));
	}

	const char* name() const
	{
		return getCell(1);
	}

	const char* nameNoContinent() const
	{
		auto s = name();
		auto sep = strchr(s, '/');
		return sep ? (sep + 1) : s;
	}

	const char* comments() const
	{
		return getCell(2);
	}

	const char* caption() const
	{
		return comments() ?: nameNoContinent();
	}

	String continent() const
	{
		auto s = name();
		auto sep = strchr(s, '/');
		return sep ? String(s, sep - s) : nullptr;
	}

	bool continentIs(const String& continent) const
	{
		auto s = name();
		auto len = continent.length();
		return strncmp(s, continent.c_str(), len) == 0 && s[len] == '/';
	}

	String getContinentCaption() const
	{
		return getContinentCaption(continent());
	}

	static String getContinentCaption(const String& name);

private:
	const char* getCell(unsigned i) const
	{
		return row ? (*row)[i] : nullptr;
	}

	const CStringArray* row{};
};

/**
 * @brief Access zone table entries stored in CSV format
 */
class ZoneTable
{
public:
	void load(const String& filename);

	/**
	 * @brief Reset to start of table
	 */
	void reset()
	{
		if(csv) {
			csv->reset();
		}
	}

	/**
	 * @brief Fetch next record
	 */
	Zone next()
	{
		if(csv && csv->next()) {
			return Zone(csv->getRow());
		}
		return Zone();
	}

private:
	std::unique_ptr<CsvReader> csv;
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
