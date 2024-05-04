#pragma once

#include <Data/CsvReader.h>
#include <memory>

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
		String s = getRow(0);
		s.replace(',', '\0');
		return CStringArray(std::move(s));
	}

	const char* name() const
	{
		return getRow(1);
	}

	const char* nameNoContinent() const
	{
		auto s = name();
		auto sep = strchr(s, '/');
		return sep ? (sep + 1) : s;
	}

	const char* comments() const
	{
		return getRow(2);
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
	const char* getRow(unsigned i) const
	{
		return row ? (*row)[i] : nullptr;
	}

	const CStringArray* row{};
};

class ZoneTable
{
public:
	void load(const String& filename);

	void reset()
	{
		if(csv) {
			csv->reset();
		}
	}

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
