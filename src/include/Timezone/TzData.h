#pragma once

#include <Data/CStringArray.h>
#include <DateTime.h>
#include <Print.h>
#include <memory>

namespace TzData
{
/**
 * @brief Index or pointer to shared string memory
 */
using StrPtr = uint8_t;

struct Year {
	static constexpr uint16_t min{0};
	static constexpr uint16_t max{9999};

	uint16_t value{min};

	constexpr Year()
	{
	}

	constexpr Year(uint16_t value) : value(value)
	{
	}

	explicit Year(const char* s, Year from = {});

	constexpr operator uint16_t() const
	{
		return value;
	}
};

struct Month {
	uint8_t value{dtJanuary};

	constexpr Month()
	{
	}

	constexpr Month(dtMonth_t value) : value(value)
	{
	}

	explicit constexpr Month(unsigned value) : value(static_cast<dtMonth_t>(value))
	{
	}

	Month(const char* s);

	constexpr operator dtMonth_t() const
	{
		return dtMonth_t(value);
	}

	explicit operator String() const
	{
		return DateTime::getIsoMonthName(value);
	}
};

struct DayOfWeek {
	uint8_t value{dtSunday};

	constexpr DayOfWeek()
	{
	}

	constexpr DayOfWeek(dtDays_t value) : value(value)
	{
	}

	explicit constexpr DayOfWeek(unsigned value) : value(static_cast<dtDays_t>(value))
	{
	}

	DayOfWeek(const char* s);

	constexpr operator dtDays_t() const
	{
		return dtDays_t(value);
	}

	explicit operator String() const
	{
		return DateTime::getIsoDayName(value);
	}
};

struct Date {
	Year year;
	Month month;
	uint8_t day{1};
};

struct On {
	enum class Kind : uint8_t {
		day,
		lastDay,
		lessOrEqual,
		greaterOrEqual,
	};
	Kind kind{Kind::day};
	uint8_t dayOfMonth{1};
	DayOfWeek dayOfWeek{};

	constexpr On()
	{
	}

	On(const char* s);

	explicit operator String() const;

	Date getDate(Year year, Month month) const;
};

struct At {
	enum TimeFormat : char {
		wall = 'w',
		std = 's',
		utc = 'u',
	};

	uint8_t hour{0};
	uint8_t min{0};
	uint8_t sec{0};
	TimeFormat timefmt{};

	constexpr At()
	{
	}

	At(const char* s);

	explicit operator String() const;
};

struct Until {
	Year year;
	Month month;
	On day;
	At time;

	constexpr Until()
	{
	}

	Until(const char* year, const char* month, const char* day, const char* time)
		: year(year, Year::max), month(month), day(day), time(time)
	{
	}

	constexpr explicit operator bool() const
	{
		return year != 0;
	}

	explicit operator String() const;
};

struct TimeOffset {
	int seconds : 31;
	int is_dst : 1;

	constexpr TimeOffset() : seconds(0), is_dst(0)
	{
	}

	TimeOffset(const char* s);

	explicit operator String() const;
};

static_assert(sizeof(Year) == 2);
static_assert(sizeof(Month) == 1);
static_assert(sizeof(On) == 3);
static_assert(sizeof(At) == 4);
static_assert(sizeof(Until) == 10);
static_assert(sizeof(TimeOffset) == 4);
static_assert(sizeof(StrPtr) == 1);

struct Rule {
	struct Line {
		Year from;
		Year to;
		Month in;
		On on;
		At at;
		TimeOffset save;
		StrPtr letters;

		String toString(const CStringArray& strings) const;
	};
	static_assert(sizeof(Line) == 20);

	String name;
	std::unique_ptr<Line[]> lines;
	uint16_t numLines;

	Rule(const char* name, uint16_t numLines) : name(name), lines(new Line[numLines]), numLines(numLines)
	{
	}

	bool operator==(const char* str) const
	{
		return name == str;
	}
};

struct Era {
	TimeOffset stdoff;
	Until until;
	StrPtr format;
	Rule* rule{};
	TimeOffset dstoff;
};

static_assert(sizeof(Era) == 24);

struct TimeZone {
	String name;
	std::unique_ptr<Era[]> eras;
	unsigned numEras{0};
};

} // namespace TzData
