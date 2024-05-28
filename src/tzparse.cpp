/*
 * Code from newlib tzset_r.c module (revised)
 *
 * Original implementation used sscanf (ssiscanf) which results in about 6K code size increase.
 */

#include "include/Timezone.h"

namespace
{
bool skip(const char*& ptr, char c)
{
	if(*ptr != c) {
		return false;
	}
	++ptr;
	return true;
}

TZ::Rule::Tag parseTag(const char*& ptr)
{
	auto start = ptr;
	size_t len;

	/* allow POSIX angle bracket < > quoted signed alphanumeric tz abbr e.g. <MESZ+0330> */
	if(skip(start, '<')) {
		ptr = start;
		while(*ptr && *ptr != '>') {
			++ptr;
		}
		len = ptr - start;
		++ptr; // close quote '>'
	} else {
		/* allow POSIX unquoted alphabetic tz abbr e.g. MESZ */
		while(isalpha(*ptr)) {
			++ptr;
		}
		len = ptr - start;
	}

	return TZ::Rule::Tag::fromString(start, len);
}

bool parseNum(const char*& ptr, uint8_t& value)
{
	if(!isdigit(*ptr)) {
		return false;
	}
	value = *ptr - '0';
	++ptr;
	if(isdigit(*ptr)) {
		value *= 10;
		value += *ptr - '0';
		++ptr;
	}
	return true;
}

struct Triplet {
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t count;
};

Triplet parseTriplet(const char*& ptr, char sep)
{
	Triplet t{};
	if(!parseNum(ptr, t.a)) {
		return t;
	}
	++t.count;
	if(!skip(ptr, sep)) {
		return t;
	}
	parseNum(ptr, t.b);
	++t.count;
	if(!skip(ptr, sep)) {
		return t;
	}
	parseNum(ptr, t.c);
	++t.count;
	return t;
}

bool parseTime(const char*& ptr, int16_t& minutes)
{
	int sign = 1;
	if(skip(ptr, '-')) {
		sign = -1;
	} else {
		skip(ptr, '+');
	}

	auto t = parseTriplet(ptr, ':');
	minutes = sign * (t.a * MINS_PER_HOUR + t.b);
	return t.count != 0;
}

} // namespace

namespace TZ
{
bool parseRule(const char*& ptr, Rule& rule)
{
	if(!skip(ptr, ',')) {
		return false;
	}

	if(!skip(ptr, 'M')) {
		// Other forms are obsolete
		return false;
	}

	auto t = parseTriplet(ptr, '.');
	if(t.count != 3 || t.a == 0 || t.b == 0) {
		return false;
	}
	rule.month = month_t(t.a - 1);
	rule.week = week_t(t.b - 1);
	rule.dow = dow_t(t.c);

	if(!skip(ptr, '/')) {
		// default time is 02:00 am
		rule.time.minutes = 120;
		return true;
	}

	return parseTime(ptr, rule.time.minutes);
}

bool parseTzstr(const char* tzstr, Rule& dst, Rule& std)
{
	if(!tzstr) {
		dst = std = Rule::UTC;
		return true;
	}

	dst = std = Rule{};

	auto ptr = tzstr;

	/* ignore implementation-specific format specifier */
	skip(ptr, ':');

	std.tag = parseTag(ptr);
	if(!std.tag[0]) {
		return false;
	}

	int16_t mins;
	if(!parseTime(ptr, mins)) {
		return false;
	}
	std.offsetMins = -mins;

	dst.tag = parseTag(ptr);
	if(!dst.tag[0]) {
		// No DST
		dst = std;
		return true;
	}

	/* otherwise we have a dst name, look for the offset */
	if(parseTime(ptr, mins)) {
		dst.offsetMins = -mins;
	} else {
		dst.offsetMins = std.offsetMins + MINS_PER_HOUR;
	}

	return parseRule(ptr, dst) && parseRule(ptr, std);
}

} // namespace TZ
