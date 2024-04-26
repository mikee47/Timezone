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

static unsigned getYear(time_t t)
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

	const TimeChangeRule& tcr = utcIsDST(utc) ? dstRule : stdRule;

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

	return local - (locIsDST(local) ? dstRule.offset : stdRule.offset) * SECS_PER_MIN;
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
	dstStartUTC = dstRule(yr) - stdRule.offset * SECS_PER_MIN;
	stdStartUTC = stdRule(yr) - dstRule.offset * SECS_PER_MIN;
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
	dt.Hour = hour;
	dt.Minute = minute;
	dt.Second = 0;
	dt.Day = 1;
	dt.Month = m - month_t::Jan; // Zero-based
	dt.Year = year;
	time_t t = dt;

	// add offset from the first of the month to r.dow, and offset for the given week
	t += ((dow - dayOfWeek(t) + 7) % 7 + w * 7) * SECS_PER_DAY;
	// back up a week if this is a "Last" rule
	if(week == week_t::Last) {
		t -= 7 * SECS_PER_DAY;
	}

	return t;
}

time_t Timezone::getNextChange(time_t utcFrom, const TimeChangeRule** rule)
{
	auto year = getYear(utcFrom);

	time_t nextDst = dstRule(year);
	if(nextDst <= utcFrom) {
		nextDst = dstRule(year + 1);
	}
	time_t nextStd = stdRule(year);
	if(nextStd <= utcFrom) {
		nextStd = stdRule(year + 1);
	}

	if(nextDst < nextStd) {
		if(rule) {
			*rule = &dstRule;
		}
		return nextDst;
	}

	if(rule) {
		*rule = &stdRule;
	}
	return nextStd;
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
