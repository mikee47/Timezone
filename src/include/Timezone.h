/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * Timezone.h - Time Zone support library
 *
 * Original code (c) Jack Christensen Mar 2012
 * Arduino Timezone Library Copyright (C) 2018 by Jack Christensen and
 * licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
 *
 * @author mikee47 <mike@sillyhouse.net>
 * July 2018 Ported to Sming
 *
 ****/

#pragma once

#include <DateTime.h>
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

	char tag[6]; ///< e.g. DST, UTC, etc.
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
	 * @brief Convert the given UTC time to local time, standard or daylight time
	 * @param utc Time in UTC
	 * @param rule Optionally return the rule used to convert the time
	 * @retval time_t The local time
	 */
	time_t toLocal(time_t utc, const Rule** rule = nullptr);

	/**
	 * @brief Convert the given local time to UTC time.
	 * @param local Local time
	 * @retval time_t Time in UTC
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
	time_t toUTC(time_t local);

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

using Timezone = TZ::Timezone;
using TimeChangeRule SMING_DEPRECATED = TZ::Rule;
