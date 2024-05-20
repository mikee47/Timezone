/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * Timezone.cpp
 *
 * Original code (c) Jack Christensen Mar 2012
 * Arduino Timezone Library Copyright (C) 2018 by Jack Christensen and
 * licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
 *
 * @author mikee47 <mike@sillyhouse.net>
 * July 2018 Ported to Sming
 *
 ****/

#include "include/Timezone.h"

static uint16_t getYear(time_t t)
{
	return DateTime(t).Year;
}

time_t Timezone::toLocal(time_t utc, const TimeChangeRule** rule)
{
	// recalculate the time change points if needed
	auto y = getYear(utc);
	if(y != getYear(dstStartUTC)) {
		calcTimeChanges(y);
	}

	auto& tcr = getRule(utcIsDST(utc));

	if(rule) {
		*rule = &tcr;
	}

	return utc + (tcr.offset * SECS_PER_MIN);
}

time_t Timezone::toUTC(time_t local)
{
	// recalculate the time change points if needed
	auto y = getYear(local);
	if(y != getYear(dstStartLoc)) {
		calcTimeChanges(y);
	}

	return local - getRule(locIsDST(local)).offsetSecs();
}

bool Timezone::utcIsDST(time_t utc)
{
	// recalculate the time change points if needed
	auto y = getYear(utc);
	if(y != getYear(dstStartUTC)) {
		calcTimeChanges(y);
	}

	// daylight time not observed in this tz
	if(stdStartUTC == dstStartUTC) {
		return false;
	}

	// northern hemisphere
	if(stdStartUTC > dstStartUTC) {
		return (utc >= dstStartUTC) && (utc < stdStartUTC);
	}

	// southern hemisphere
	return (utc >= dstStartUTC) || (utc < stdStartUTC);
}

bool Timezone::locIsDST(time_t local)
{
	// recalculate the time change points if needed
	auto y = getYear(local);
	if(y != getYear(dstStartLoc)) {
		calcTimeChanges(y);
	}

	// daylight time not observed in this tz
	if(stdStartUTC == dstStartUTC) {
		return false;
	}

	// northern hemisphere
	if(stdStartLoc > dstStartLoc) {
		return (local >= dstStartLoc && local < stdStartLoc);
	}

	// southern hemisphere
	return (local >= dstStartLoc) || (local < stdStartLoc);
}

void Timezone::calcTimeChanges(unsigned yr)
{
	dstStartLoc = dstRule(yr);
	stdStartLoc = stdRule(yr);
	dstStartUTC = dstStartLoc - stdRule.offset * SECS_PER_MIN;
	stdStartUTC = stdStartLoc - dstRule.offset * SECS_PER_MIN;
}

time_t TimeChangeRule::operator()(unsigned year) const
{
	// working copies of r.month and r.week which we may adjust
	uint8_t m = month;
	uint8_t w = week;

	// is this a "Last week" rule?
	if(week == week_t::Last) {
		// yes, for "Last", go to the next month
		if(++m > month_t::Dec) {
			m = month_t::Jan;
			++year;
		}
		// and treat as first week of next month, subtract 7 days later
		w = week_t::First;
	}

	// calculate first day of the month, or for "Last" rules, first day of the next month
	DateTime dt;
	dt.Day = 1;
	dt.Month = m - month_t::Jan; // Zero-based
	dt.Year = year;
	time_t t = dt;

	// add offset from the first of the month to r.dow, and offset for the given week
	t += ((dow - dayOfWeek(t) + 7) % DAYS_PER_WEEK + (w - week_t::First) * DAYS_PER_WEEK) * SECS_PER_DAY;
	// back up a week if this is a "Last" rule
	if(week == week_t::Last) {
		t -= DAYS_PER_WEEK * SECS_PER_DAY;
	}

	return t + int(time.minutes) * SECS_PER_MIN;
}

void Timezone::init(const TimeChangeRule& dstStart, const TimeChangeRule& stdStart)
{
	dstRule = dstStart;
	stdRule = stdStart;
	// force calcTimeChanges() at next conversion call
	dstStartUTC = 0;
	stdStartUTC = 0;
	dstStartLoc = 0;
	stdStartLoc = 0;
}
