#include "include/Timezone/TzData.h"

namespace TzData
{
Month matchMonth(const char* ptr)
{
	if(!ptr) {
		return dtJanuary;
	}
	switch(*ptr) {
	case 'J':
		return ptr[1] == 'a' ? dtJanuary : ptr[2] == 'n' ? dtJune : dtJuly;
	case 'F':
		return dtFebruary;
	case 'M':
		return ptr[2] == 'r' ? dtMarch : dtMay;
	case 'A':
		return ptr[1] == 'p' ? dtApril : dtAugust;
	case 'S':
		return dtSeptember;
	case 'O':
		return dtOctober;
	case 'N':
		return dtNovember;
	case 'D':
		return dtDecember;
	default:
		return dtJanuary;
	}
}

DayOfWeek matchDay(const char* ptr)
{
	if(!ptr) {
		return dtSunday;
	}
	switch(*ptr) {
	case 'S':
		return ptr[1] == 'a' ? dtSaturday : dtSunday;
	case 'M':
		return dtMonday;
	case 'T':
		return ptr[1] == 'u' ? dtTuesday : dtThursday;
	case 'W':
		return dtWednesday;
	case 'F':
		return dtFriday;
	}
	return dtSunday;
}

Year::Year(const char* ptr, Year from)
{
	if(!ptr || *ptr == 'o') {
		value = from;
	} else if(*ptr == 'm') {
		value = (ptr[1] == 'i') ? min : max;
	} else {
		value = atoi(ptr);
	}
}

Month::Month(const char* s) : value(matchMonth(s))
{
}

DayOfWeek::DayOfWeek(const char* s) : value(matchDay(s))
{
}

Date On::getDate(Year year, Month month) const
{
	if(kind == Kind::day) {
		return Date{year, month, dayOfMonth};
	}

	uint8_t day = (kind == Kind::lastDay) ? DateTime::getMonthDays(month, year) : dayOfMonth;
	time_t time = DateTime::toUnixTime(0, 0, 0, day, month, year);
	int diff = dayOfWeek - DateTime(time).DayofWeek;
	if(kind == Kind::greaterOrEqual) {
		if(diff < 0) {
			diff += 7;
		}
	} else if(diff > 0) {
		diff -= 7;
	}
	time += diff;
	DateTime dt(time);
	return Date{dt.Year, Month(dt.Month), dt.Day};
}

On::On(const char* s)
{
	if(!s) {
		return;
	}

	if(isdigit(*s)) {
		// 5        the fifth of the month
		kind = Kind::day;
		dayOfMonth = atoi(s);
		return;
	}

	if(memcmp(s, "last", 4) == 0) {
		// lastSun  the last Sunday in the month
		// lastMon  the last Monday in the month
		kind = Kind::lastDay;
		dayOfWeek = s + 4;
		return;
	}

	// Sun >= 8 first Sunday on or after the eighth
	// Sun <= 25 last Sunday on or before the 25th
	// The “<=” and “>=” constructs can result in a day in the neighboring month; for example, the
	// IN - ON combination “Oct Sun >= 31” stands for the first Sunday on or after October 31,
	// even if that Sunday occurs in November.
	dayOfWeek = s;
	while(isalpha(*s)) {
		++s;
	}
	dayOfMonth = atoi(s + 2);
	kind = (*s == '>') ? Kind::greaterOrEqual : Kind::lessOrEqual;
}

On::operator String() const
{
	String s;
	switch(kind) {
	case Kind::day:
		s += String(dayOfMonth);
		break;
	case Kind::lastDay:
		s += "last";
		s += String(dayOfWeek);
		break;
	case Kind::lessOrEqual:
		s += String(dayOfWeek);
		s += "<=";
		s += String(dayOfMonth);
		break;
	case Kind::greaterOrEqual:
		s += String(dayOfWeek);
		s += ">=";
		s += String(dayOfMonth);
		break;
	}
	return s;
}

At::At(const char* s)
{
	if(!s) {
		return;
	}

	char* p;
	hour = strtol(s, &p, 10);
	if(*p == ':') {
		++p;
		min = strtol(p, &p, 10);
		if(*p == ':') {
			++p;
			sec = strtol(p, &p, 10);
		}
	}

	switch(*p) {
	case 's':
		timefmt = TimeFormat::std;
		break;
	case 'u':
	case 'g':
	case 'z':
		timefmt = TimeFormat::utc;
		break;
	case 'w':
	case '\0':
	default:
		timefmt = TimeFormat::wall;
	}
}

At::operator String() const
{
	char buf[10];
	m_snprintf(buf, sizeof(buf), "%02u:%02u:%02u%c", hour, min, sec, timefmt);

	return buf;
}

Until::operator String() const
{
	String s;
	s += String(day);
	s += ' ';
	s += String(month);
	s += ' ';
	s += year;
	s += ' ';
	s += String(time);
	return s;
}

TimeOffset::TimeOffset(const char* s)
{
	if(!s) {
		return;
	}

	bool neg = (*s == '-');
	if(neg) {
		++s;
	}
	char* p;
	seconds = strtol(s, &p, 10) * 3600;
	if(*p == ':') {
		++p;
		seconds += strtol(p, &p, 10) * 60;
		if(*p == ':') {
			++p;
			seconds += strtol(p, &p, 10);
		}
	}
	if(neg) {
		seconds = -seconds;
	}

	// timefmt
	if(*p == 'd') {
		is_dst = true;
	} else {
		is_dst = (seconds != 0);
	}
}

TimeOffset::operator String() const
{
	auto secs = abs(seconds);
	auto mins = secs / 60;
	auto hours = mins / 60;
	mins %= 60;
	secs %= 60;
	char sign = (seconds < 0) ? '-' : '+';

	char buf[16];
	m_snprintf(buf, sizeof(buf), "%c%u:%02u:%02u", sign, hours, mins, secs);
	return buf;
}

Rule::Line::operator String() const
{
	String s;
	s += String(from);
	s += ' ';
	s += String(to);
	s += ' ';
	s += String(in);
	s += ' ';
	s += String(on);
	s += ' ';
	s += String(at);
	s += ' ';
	s += String(save);
	s += " #";
	s += String(letters);
	return s;
}

} // namespace TzData
