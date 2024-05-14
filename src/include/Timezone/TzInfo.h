#pragma once

#include "CsvTable.h"
#include "TzData.h"
#include <WVector.h>

/**
 * @brief Base type for reading .zi records
 *
 * Use `ZoneRecord`, `EraRecord` or `LinkRecord` to access according to type.
 */
struct TzInfoRecord : public CsvRecord {
	enum class Type : char {
		invalid,
		zone,
		era,
		link,
		rule,
	};

	Type type() const
	{
		auto s = row[0];
		if(!s) {
			return Type::invalid;
		}
		char c = *s;
		switch(c) {
		case 'Z':
			return Type::zone;
		case 'L':
			return Type::link;
		case 'R':
			return Type::rule;
		default:
			return (c == '-') || isdigit(c) ? Type::era : Type::invalid;
		}
	}

	using CsvRecord::CsvRecord;
};

using TzInfoTable = CsvTable<TzInfoRecord>;

struct ZoneRecord {
	enum ColumnIndex {
		col_type,	 // Z
		col_location, // Without area
	};

	const CStringArray& row;

	ZoneRecord(const TzInfoRecord& rec) : row(rec.row)
	{
	}

	const char* location() const
	{
		return row[col_location];
	}
};

struct EraRecord {
	enum ColumnIndex {
		col_stdoff,
		col_rule,
		col_format,
		col_year,
		col_month,
		col_day,
		col_time,
	};

	const CStringArray& row;

	EraRecord(const TzInfoRecord& rec) : row(rec.row)
	{
	}

	TzData::TimeOffset stdoff() const
	{
		return row[col_stdoff];
	}

	const char* rule() const
	{
		return row[col_rule];
	}

	const char* format() const
	{
		return row[col_format];
	}

	TzData::Year year() const
	{
		return TzData::Year(row[col_year], TzData::Year::max);
	}

	TzData::Month month() const
	{
		return row[col_month];
	}

	TzData::On day() const
	{
		return row[col_day];
	}

	TzData::At time() const
	{
		return row[col_time];
	}

	TzData::Until until() const
	{
		return {row[col_year], row[col_month], row[col_day], row[col_time]};
	}

	enum class RuleKind {
		none,
		time, // Contains a TimeOffset value
		rule, // External rule requires lookup
	};

	RuleKind ruleKind() const
	{
		auto s = rule();
		if(!s) {
			return RuleKind::none;
		}
		if(s[0] == '-') {
			return s[1] ? RuleKind::time : RuleKind::none;
		}
		return RuleKind::rule;
	}
};

struct LinkRecord {
	enum ColumnIndex {
		col_type, // L
		col_zone,
		col_location, // Without area
	};

	const CStringArray& row;

	LinkRecord(const TzInfoRecord& rec) : row(rec.row)
	{
	}

	const char* type() const
	{
		return row[col_type];
	}

	const char* zone() const
	{
		return row[col_zone];
	}

	const char* location() const
	{
		return row[col_location];
	}
};

struct RuleRecord {
	enum ColumnIndex {
		col_type, // R
		col_name,
		col_from,
		col_to,
		col_unused,
		col_in,
		col_on,
		col_at,
		col_save,
		col_letters,
	};

	const CStringArray& row;

	RuleRecord(const TzInfoRecord& rec) : row(rec.row)
	{
	}

	// NOTE: type and name are omitted as we store each rule in its own file
	const char* name() const
	{
		return row[col_name];
	}

	TzData::Year from() const
	{
		return TzData::Year(row[col_from]);
	}

	TzData::Year to() const
	{
		return TzData::Year(row[col_to], from());
	}

	TzData::Month in() const
	{
		return row[col_in];
	}

	TzData::On on() const
	{
		return row[col_on];
	}

	TzData::At at() const
	{
		return row[col_at];
	}

	TzData::TimeOffset save() const
	{
		return row[col_save];
	}

	const char* letters() const
	{
		return row[col_letters];
	}

	size_t printTo(Print& p) const
	{
		size_t n{0};
		n += p.print(from());
		n += p.print(' ');
		n += p.print(to());
		n += p.print(' ');
		n += p.print(String(in()));
		n += p.print(' ');
		n += p.print(String(on()));
		n += p.print(' ');
		n += p.print(String(at()));
		n += p.print(' ');
		n += p.print(String(save()));
		n += p.print(' ');
		n += p.print(letters());
		return n;
	}
};

class ZoneData
{
public:
	// Locate zone in table, return matched name
	String findZone(const String& name, bool includeLinks = true);

	static void normalize(String& name);

	static String normalize(const String& name)
	{
		String s = name;
		normalize(s);
		return s;
	}

	/**
	 * @brief Split a zone name into area/location
	 * @param name On return, contains just the location
	 * @retval String Zone area
	 */
	static String splitName(String& name);

	// private:
	void scanZone();

	TzData::StrPtr getStrPtr(const char* str);

	TzData::Rule* loadRule(const char* name);

	std::unique_ptr<TzInfoTable> zoneTable;
	String currentArea;
	TzData::TimeZone timezone;
	Vector<TzData::Rule> rules;
	CStringArray strings;
};

/**
 * @brief to2050.tzs record
 *
 * Contains output from `zdump -i -c 2050` which we can use to verify code.
 */
struct TzsRecord {
	enum ColumnIndex {
		col_date,
		col_time,
		col_interval,
		col_tag,
		col_isdst,
	};

	const CStringArray& row;

	TzsRecord(const TzInfoRecord& rec) : row(rec.row)
	{
	}

	TzData::Date date() const
	{
		auto s = row[col_date];
		if(!s || *s == '-') {
			return {};
		}
		DateTime dt;
		dt.fromISO8601(s);
		return {dt.Year, TzData::Month(dt.Month), dt.Day};
	}

	TzData::At time() const
	{
		auto s = row[col_time];
		return s && *s != '-' ? s : nullptr;
	}

	DateTime datetime() const
	{
		String buf;
		auto s = row[col_date];
		if(s && *s != '-') {
			buf += s;
		}
		buf += 'T';
		buf += String(time());
		DateTime dt;
		dt.fromISO8601(buf);
		return dt;
	}

	TzData::TimeOffset interval() const
	{
		return TzData::TimeOffset(row[col_interval]);
	}

	const char* tag() const
	{
		return row[col_tag];
	}

	const char* isdst() const
	{
		return row[col_isdst];
	}
};
