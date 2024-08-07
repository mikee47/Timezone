/****
 * Timezone.h - Time Zone support library
 *
 * Original code (c) Jack Christensen Mar 2012
 * Arduino Timezone Library Copyright (C) 2018 by Jack Christensen and
 * licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
 *
 * This file is part of the Timezone Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 * @author mikee47 <mike@sillyhouse.net>
 * July 2018 Ported to Sming
 * May 2024 Added Posix TZ string and IANA database support
 *
 ****/

#pragma once

#include <ZonedTime.h>
#include <Print.h>
#include <limits>

namespace TZ
{
/**
 * @brief Week number for `Rule`
 */
enum week_t {
	First,
	Second,
	Third,
	Fourth,
	Last,
};

/**
 * @brief Day of week. Same as DateTime dtDays_t.
 */
enum dow_t {
	Sun = 0,
	Mon,
	Tue,
	Wed,
	Thu,
	Fri,
	Sat,
};

/**
 * @brief Month by name. Same as DateTime dtMonth_t.
 */
enum month_t {
	Jan = 0,
	Feb,
	Mar,
	Apr,
	May,
	Jun,
	Jul,
	Aug,
	Sep,
	Oct,
	Nov,
	Dec,
};

/**
 * @brief Earliest timestamp we might wish to use
 * @note
 * 	 64-bit  -5364662400  "1800-01-01 00:00:00"
 *   32-bit  -2147483647  "1901-12-31 20:45:53"
 * @todo Simplify this expression once 32-bit time_t has been eradicated.
 */
static constexpr time_t minTime = std::max(-5364662400LL, (long long)std::numeric_limits<time_t>::min() + 1);

/**
 * @brief Largest future timestamp value we could reasonably want
 * @note
 * 	 64-bit  253402300799  "9999-12-31 23:59:59"
 *   32-bit  2147483646    "2038-01-19 03:14:06"
 * @todo Simplify this expression once 32-bit time_t has been eradicated.
 */
static constexpr time_t maxTime = std::min(253402300799LL, (long long)std::numeric_limits<time_t>::max() - 1);

/**
 * @brief Value outside normal range used to indicate abnormal or uninitialised time values
 */
static constexpr time_t invalidTime = maxTime + 1;

/**
 * @brief Describe rules for when daylight/summer time begins, and when standard time begins
 *
 * This rule structure is an exact analogue of the POSIX 'M'-style rules, which are the only ones
 * in general use. The GLIBC manual page provides a good overview of these rules:
 *
 * 		https://sourceware.org/glibc/manual/2.39/html_node/TZ-Variable.html
 *
 * Original versions of this library only allowed a single value for hours, for example:
 *
 * 		{"BST", Last, Sun, Mar, 1, 60}
 *
 * However, some zones also require a minute value, such as Pacific/Chatham which changes at 03:45.
 * We can use a fractional value (3.75) for this:
 *
 * 		{"+1245", First, Sun, Apr, 3.75, 765}
 *
 * Western greenland has a negative hours value, America/Godthab:
 *
 *		{"-01", Last, Sun, Mar, -1, -60}
 *
 * Note that at time of writing newlib (the embedded C library) does not support negative time values
 * (via tzset) and produces incorrect results.
 *
 */
struct Rule {
	struct Time {
		int16_t minutes;

		constexpr Time(float hours = 0) : minutes(hours * MINS_PER_HOUR)
		{
		}
	};

	using Tag = DateTime::ZoneInfo::Tag;
	Tag tag; ///< e.g. DST, UTC, etc.
	week_t week : 4;
	dow_t dow : 4;
	month_t month : 8;
	Time time;
	int16_t offsetMins; ///< Offset from UTC

	int offsetSecs() const
	{
		return int(offsetMins) * SECS_PER_MIN;
	}

	/**
	 * @brief Convert the given time change rule to a time_t value for the given year
	 * @param year Which year to work with
	 * @retval time_t Timestamp for the given time change
	 */
	time_t operator()(unsigned year) const;

	/**
	 * @brief Obtain a numeric value for comparison purposes
	 */
	explicit operator int() const
	{
		return (month << 6) | (week << 3) | dow;
	}

	bool operator==(const Rule& other) const
	{
		return this == &other || memcmp(this, &other, sizeof(other)) == 0;
	}

	bool operator!=(const Rule& other) const
	{
		return !operator==(other);
	}

	static const Rule UTC;
};

#ifndef __WIN32
static_assert(sizeof(Rule) == 12, "Rule size unexpected");
#endif

/**
 * @brief Class to support local/UTC time conversions using rules
 */
class Timezone
{
public:
	Timezone()
	{
	}

	/**
	 * @brief Create a timezone with daylight savings
	 * @param dstStart Rule giving start of daylight savings
	 * @param stdStart Rule giving start of standard time
	 * @note If both rules are the same then the zone operates in permanent standard time
	 */
	Timezone(const Rule& dstStart, const Rule& stdStart)
		: dstRule(dstStart), stdRule(stdStart), hasDst(dstRule.tag[0] && dstRule != stdRule)
	{
	}

	/**
	 * @brief Create a timezone which has no daylight savings
	 * @param std Rule describing standard time
	 * @note Only tag and offset fields from rule are significant
	 */
	explicit Timezone(const Rule& std) : dstRule(std), stdRule(std), hasDst(false)
	{
	}

	/**
	 * @deprected Use copy assignment, e.g. `tz = Timezone(...)`
	 */
	void init(const Rule& dstStart, const Rule& stdStart) SMING_DEPRECATED
	{
		*this = Timezone(dstStart, stdStart);
	}

	/**
	 * @name Construct a Timezone from a POSIX rule string
	 * @{
	*/
	static Timezone fromPosix(const char* tzstr);

	static Timezone fromPosix(const String& tzstr)
	{
		return fromPosix(tzstr.c_str());
	}
	/** @} */

	explicit operator bool() const
	{
		return *stdRule.tag != '\0';
	}

	/**
	 * @brief Convert the given UTC time to local time, standard or daylight time
	 * @param utc Time in UTC
	 * @param rule Optionally return the rule used to convert the time
	 * @retval time_t The local time
	 */
	time_t toLocal(time_t utc, const Rule** rule = nullptr);

	/**
	 * @brief Obtain a ZonedTime instance for the given UTC
	 * @param utc Time in UTC
	 * @param beforeTransition If time is exactly on a transition to/from daylight savings then
	 * this determines whether the returned information contains the local time prior to the change
	 * or after the change.
	 * @retval ZonedTime Contains the UTC time given plus offset, etc. at that point in time
	 */
	ZonedTime makeZoned(time_t utc, bool beforeTransition = false);

	/**
	 * @brief Convert the given local time to UTC time.
	 * @param local Local time
	 * @retval ZonedTime Contains UTC point in time with associated local offset, etc.
	 * @note
	 *
	 * WARNING:
     * This function is provided for completeness, but should seldom be
     * needed and should be used sparingly and carefully.
     *
     * Ambiguous situations occur after the Standard-to-DST and the
     * DST-to-Standard time transitions. When changing to DST, there is
     * one hour of local time that does not exist, since the clock moves
     * forward one hour. Similarly, when changing to standard time, there
     * is one hour of local times that occur twice since the clock moves
     * back one hour.
     *
     * This function does not test whether it is passed an erroneous time
     * value during the Local -> DST transition that does not exist.
     * If passed such a time, an incorrect UTC time value will be returned.
     *
     * If passed a local time value during the DST -> Local transition
     * that occurs twice, it will be treated as the earlier time, i.e.
     * the time that occurs before the transition.
     *
     * Calling this function with local times during a transition interval
     * should be avoided.
	 */
	ZonedTime toUTC(time_t local);

	/**
	 * @brief Determine whether the UTC time is within the DST interval or the Standard time interval
	 * @param utc
	 * @retval bool true if time is within DST
	 */
	bool utcIsDST(time_t utc);

	/**
	 * @brief Determine whether the given local time is within the DST interval or the Standard time interval.
	 * @param local Local time
	 * @retval bool true if time is within DST
	 */
	bool locIsDST(time_t local);

	/**
	 * @brief Return the appropriate dalight-savings tag to append to displayed times
	 * @param isDst true if DST tag is required, otherwise non-DST tag is returned
	 * @retval const char* The tag
	 */
	const char* timeTag(bool isDst) const
	{
		return isDst ? dstRule.tag : stdRule.tag;
	}

	/**
	 * @brief Return the appropriate time tag for a UTC time
	 * @param utc The time in UTC
	 * @retval const char* Tag, such as UTC, BST, etc.
	 */
	const char* utcTimeTag(time_t utc)
	{
		return timeTag(utcIsDST(utc));
	}

	/**
     * @brief Return the appropriate time tag for a local time
     * @param local The local time
     * @retval const char* Tag, such as UTC, BST, etc.
     */
	const char* localTimeTag(time_t local)
	{
		return timeTag(locIsDST(local));
	}

	/**
	 * @brief Determine when the next change to/from DST is
	 * @param utcFrom Point in time *after* which change is to occur
	 * @retval ZonedTime UTC time When the change will occur, or maxTime if there is no DST in effect.
	 * ZonedTime::local() returns the *new* local time at the transition.
	 */
	ZonedTime getNextChange(time_t utcFrom);

	/**
	 * @brief Get transition time for the given year
	 * @param year
	 * @param toDst true to obtain STD->DST transition, false for DST->STD
	 * @retval ZonedTime Time of transition, or maxTime if there is no DST in effect.
	 * ZonedTime::local() returns the *new* local time at the transition.
	 */
	ZonedTime getTransition(uint16_t year, bool toDst);

	/**
	 * @brief Get reference to a timechange rule
	 * @param isDst true for DST rule, false for STD rule
	 * @retval Rule
	 */
	const Rule& getRule(bool isDst) const
	{
		return isDst ? dstRule : stdRule;
	}

	/**
	 * @brief If dst and std rules are the same we do not use daylight savings
	 */
	bool hasDaylightSavings() const
	{
		return hasDst;
	}

	String toString() const;

	size_t printTo(Print& p) const;

private:
	/*
	 * Calculate the DST and standard time change points for the given
	 * given year as local and UTC time_t values.
	 */
	void calcTimeChanges(unsigned yr);

private:
	Rule dstRule{};					 ///< rule for start of dst or summer time for any year
	Rule stdRule{};					 ///< rule for start of standard time for any year
	time_t dstStartUTC{invalidTime}; ///< dst start for given/current year, given in UTC
	time_t stdStartUTC{invalidTime}; ///< std time start for given/current year, given in UTC
	bool hasDst{};					 ///< false if rules are the same
};

} // namespace TZ

String toString(TZ::week_t week);

using Timezone = TZ::Timezone;
using TimeChangeRule SMING_DEPRECATED = TZ::Rule;
