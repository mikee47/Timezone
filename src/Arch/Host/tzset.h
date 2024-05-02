#pragma once

#include <time.h>

namespace TZ
{
struct tzrule_struct {
	char ch;
	int m; /* Month of year if ch=M */
	int n; /* Week of month if ch=M */
	int d; /* Day of week if ch=M, day of year if ch=J or ch=D */
	int s; /* Time of day in seconds */
	time_t change;
	long offset; /* Match type of _timezone. */
};

struct tzinfo_struct {
	int tznorth;
	int tzyear;
	tzrule_struct tzrule[2];

	const tzrule_struct* __tzrule{tzrule};
};

extern const tzrule_struct tzinfo;
extern const char* g_tzname[2];
extern int g_daylight;
extern long g_timezone;

void setZone(const char* tzenv);
int calcLimits(int year);
tzinfo_struct& getInfo();

}; // namespace TZ

#define _tzname TZ::g_tzname
using __tzinfo_type = TZ::tzinfo_struct;
