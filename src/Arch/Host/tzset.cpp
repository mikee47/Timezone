/*
 * Code taken from newlib tzset_r.c module.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "tzset.h"
#include <Data/CString.h>

#define SECSPERMIN 60L
#define MINSPERHOUR 60L
#define HOURSPERDAY 24L
#define SECSPERHOUR (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY (SECSPERHOUR * HOURSPERDAY)
#define DAYSPERWEEK 7
#define MONSPERYEAR 12

#define YEAR_BASE 1900
#define EPOCH_YEAR 1970
#define EPOCH_WDAY 4
#define EPOCH_YEARS_SINCE_LEAP 2
#define EPOCH_YEARS_SINCE_CENTURY 70
#define EPOCH_YEARS_SINCE_LEAP_CENTURY 370

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

// #define sscanf siscanf /* avoid to pull in FP functions. */

#define TZNAME_MIN 3  /* POSIX min TZ abbr size local def */
#define TZNAME_MAX 10 /* POSIX max TZ abbr size local def */

namespace
{
static char g_tzname_std[TZNAME_MAX + 2];
static char g_tzname_dst[TZNAME_MAX + 2];
CString prev_tzenv;

const int month_lengths[2][MONSPERYEAR]{
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

} // namespace

namespace TZ
{
const char* g_tzname[2]{"GMT", "GMT"};
tzinfo_struct g_tzinfo{1, 0, {{'J'}, {'J'}}};
int g_daylight;
long g_timezone;

tzinfo_struct& getInfo()
{
	return g_tzinfo;
}

/*
 * tzcalc_limits.c
 * Original Author: Adapted from tzcode maintained by Arthur David Olson.
 * Modifications:
 * - Changed to mktm_r and added __tzcalc_limits - 04/10/02, Jeff Johnston
 * - Fixed bug in mday computations - 08/12/04, Alex Mogilnikov <alx@intellectronika.ru>
 * - Fixed bug in __tzcalc_limits - 08/12/04, Alex Mogilnikov <alx@intellectronika.ru>
 * - Moved __tzcalc_limits() to separate file - 05/09/14, Freddie Chopin <freddie_chopin@op.pl>
 */

int calcLimits(int year)
{
	if(year < EPOCH_YEAR) {
		return 0;
	}

	const auto tz = &g_tzinfo;

	tz->tzyear = year;

	int years = (year - EPOCH_YEAR);

	int year_days = years * 365 + (years - 1 + EPOCH_YEARS_SINCE_LEAP) / 4 -
					(years - 1 + EPOCH_YEARS_SINCE_CENTURY) / 100 + (years - 1 + EPOCH_YEARS_SINCE_LEAP_CENTURY) / 400;

	for(auto& rule : tz->tzrule) {
		int days;
		if(rule.ch == 'J') {
			/* The Julian day n (1 <= n <= 365). */
			days = year_days + rule.d + (isleap(year) && rule.d >= 60);
			/* Convert to yday */
			--days;
		} else if(rule.ch == 'D') {
			days = year_days + rule.d;
		} else {
			const int yleap = isleap(year);
			int m_day, m_wday, wday_diff;
			const int* const ip = month_lengths[yleap];

			days = year_days;

			for(int j = 1; j < rule.m; ++j) {
				days += ip[j - 1];
			}

			m_wday = (EPOCH_WDAY + days) % DAYSPERWEEK;

			wday_diff = rule.d - m_wday;
			if(wday_diff < 0) {
				wday_diff += DAYSPERWEEK;
			}
			m_day = (rule.n - 1) * DAYSPERWEEK + wday_diff;

			while(m_day >= ip[rule.m - 1]) {
				m_day -= DAYSPERWEEK;
			}

			days += m_day;
		}

		/* store the change-over time in GMT form by adding offset */
		rule.change = (time_t)days * SECSPERDAY + rule.s + rule.offset;
	}

	tz->tznorth = (tz->tzrule[0].change < tz->tzrule[1].change);

	return 1;
}

void setZone(const char* tzenv)
{
	const auto tz = &g_tzinfo;
	const tzrule_struct default_tzrule{'J'};

	if(tzenv == NULL) {
		g_timezone = 0;
		g_daylight = 0;
		g_tzname[0] = "GMT";
		g_tzname[1] = "GMT";
		tz->tzrule[0] = default_tzrule;
		tz->tzrule[1] = default_tzrule;
		prev_tzenv = nullptr;
		return;
	}

	if(prev_tzenv == tzenv) {
		return;
	}

	prev_tzenv = tzenv;

	/* default to unnamed UTC in case of error */
	g_timezone = 0;
	g_daylight = 0;
	g_tzname[0] = "";
	g_tzname[1] = "";
	tz->tzrule[0] = default_tzrule;
	tz->tzrule[1] = default_tzrule;

	/* ignore implementation-specific format specifier */
	if(*tzenv == ':') {
		++tzenv;
	}

	int n;

	/* allow POSIX angle bracket < > quoted signed alphanumeric tz abbr e.g. <MESZ+0330> */
	if(*tzenv == '<') {
		++tzenv;

		/* quit if no items, too few or too many chars, or no close quote '>' */
		if(sscanf(tzenv, "%11[-+0-9A-Za-z]%n", g_tzname_std, &n) <= 0 || n < TZNAME_MIN || TZNAME_MAX < n ||
		   '>' != tzenv[n]) {
			return;
		}

		++tzenv; /* bump for close quote '>' */
	} else {
		/* allow POSIX unquoted alphabetic tz abbr e.g. MESZ */
		if(sscanf(tzenv, "%11[A-Za-z]%n", g_tzname_std, &n) <= 0 || n < TZNAME_MIN || TZNAME_MAX < n) {
			return;
		}
	}

	tzenv += n;

	int sign = 1;
	if(*tzenv == '-') {
		sign = -1;
		++tzenv;
	} else if(*tzenv == '+') {
		++tzenv;
	}

	unsigned short hh;
	unsigned short mm = 0;
	unsigned short ss = 0;

	if(sscanf(tzenv, "%hu%n:%hu%n:%hu%n", &hh, &n, &mm, &n, &ss, &n) < 1) {
		return;
	}

	long offset0 = sign * (ss + SECSPERMIN * mm + SECSPERHOUR * hh);
	tzenv += n;

	/* allow POSIX angle bracket < > quoted signed alphanumeric tz abbr e.g. <MESZ+0330> */
	if(*tzenv == '<') {
		++tzenv;

		/* quit if no items, too few or too many chars, or no close quote '>' */
		if(sscanf(tzenv, "%11[-+0-9A-Za-z]%n", g_tzname_dst, &n) <= 0 && tzenv[0] == '>') { /* No dst */
			g_tzname[0] = g_tzname_std;
			g_tzname[1] = g_tzname[0];
			tz->tzrule[0].offset = offset0;
			g_timezone = offset0;
			return;
		}
		if(n < TZNAME_MIN || TZNAME_MAX < n || '>' != tzenv[n]) { /* error */
			return;
		}

		++tzenv; /* bump for close quote '>' */
	} else {
		/* allow POSIX unquoted alphabetic tz abbr e.g. MESZ */
		if(sscanf(tzenv, "%11[A-Za-z]%n", g_tzname_dst, &n) <= 0) { /* No dst */
			g_tzname[0] = g_tzname_std;
			g_tzname[1] = g_tzname[0];
			tz->tzrule[0].offset = offset0;
			g_timezone = offset0;
			return;
		}
		if(n < TZNAME_MIN || TZNAME_MAX < n) { /* error */
			return;
		}
	}

	tzenv += n;

	/* otherwise we have a dst name, look for the offset */
	sign = 1;
	if(*tzenv == '-') {
		sign = -1;
		++tzenv;
	} else if(*tzenv == '+')
		++tzenv;

	hh = 0;
	mm = 0;
	ss = 0;

	long offset1;
	n = 0;
	if(sscanf(tzenv, "%hu%n:%hu%n:%hu%n", &hh, &n, &mm, &n, &ss, &n) <= 0) {
		offset1 = offset0 - 3600;
	} else {
		offset1 = sign * (ss + SECSPERMIN * mm + SECSPERHOUR * hh);
	}

	tzenv += n;

	for(int i = 0; i < 2; ++i) {
		if(*tzenv == ',') {
			++tzenv;
		}

		if(*tzenv == 'M') {
			unsigned short m, w, d;
			if(sscanf(tzenv, "M%hu%n.%hu%n.%hu%n", &m, &n, &w, &n, &d, &n) != 3 || m < 1 || m > 12 || w < 1 || w > 5 ||
			   d > 6) {
				return;
			}

			tz->tzrule[i].ch = 'M';
			tz->tzrule[i].m = m;
			tz->tzrule[i].n = w;
			tz->tzrule[i].d = d;

			tzenv += n;
		} else {
			int ch;
			char* end;
			if(*tzenv == 'J') {
				ch = 'J';
				++tzenv;
			} else {
				ch = 'D';
			}

			unsigned short d = strtoul(tzenv, &end, 10);

			/* if unspecified, default to US settings */
			/* From 1987-2006, US was M4.1.0,M10.5.0, but starting in 2007 is
	 		 * M3.2.0,M11.1.0 (2nd Sunday March through 1st Sunday November)  */
			if(end == tzenv) {
				if(i == 0) {
					tz->tzrule[0].ch = 'M';
					tz->tzrule[0].m = 3;
					tz->tzrule[0].n = 2;
					tz->tzrule[0].d = 0;
				} else {
					tz->tzrule[1].ch = 'M';
					tz->tzrule[1].m = 11;
					tz->tzrule[1].n = 1;
					tz->tzrule[1].d = 0;
				}
			} else {
				tz->tzrule[i].ch = ch;
				tz->tzrule[i].d = d;
			}

			tzenv = end;
		}

		/* default time is 02:00:00 am */
		hh = 2;
		mm = 0;
		ss = 0;
		n = 0;

		if(*tzenv == '/')
			if(sscanf(tzenv, "/%hu%n:%hu%n:%hu%n", &hh, &n, &mm, &n, &ss, &n) <= 0) {
				/* error in time format, restore tz rules to default and return */
				tz->tzrule[0] = default_tzrule;
				tz->tzrule[1] = default_tzrule;
				return;
			}

		tz->tzrule[i].s = ss + SECSPERMIN * mm + SECSPERHOUR * hh;

		tzenv += n;
	}

	tz->tzrule[0].offset = offset0;
	tz->tzrule[1].offset = offset1;
	g_tzname[0] = g_tzname_std;
	g_tzname[1] = g_tzname_dst;
	calcLimits(tz->tzyear);
	g_timezone = tz->tzrule[0].offset;
	g_daylight = tz->tzrule[0].offset != tz->tzrule[1].offset;
}

} // namespace TZ
