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

// convenient constants for TimeChangeRules
enum __attribute__((packed)) week_t {
	First,
	Second,
	Third,
	Fourth,
	Last,
};

enum __attribute__((packed)) dow_t {
	Sun = 0,
	Mon,
	Tue,
	Wed,
	Thu,
	Fri,
	Sat,
};

enum __attribute__((packed)) month_t {
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
struct TimeChangeRule {
	struct Time {
		int16_t minutes;

		constexpr Time(float hours = 0) : minutes(hours * MINS_PER_HOUR)
		{
		}
	};

	char tag[6];   ///< e.g. DST, UTC, etc.
	week_t week;   ///< First, Second, Third, Fourth, or Last week of the month
	dow_t dow;	 ///< Day of week, 0=Sun
	month_t month; ///< 1=Jan
	Time time;
	int offset; ///< Offset from UTC in minutes

	/**
	 * @brief Convert the given time change rule to a time_t value for the given year
	 * @param year Which year to work with
	 * @retval time_t Timestamp for the given time change
	 */
	time_t operator()(unsigned year) const;
};

/**
 * @brief Class to support local/UTC time conversions using rules
 */
class Timezone
{
public:
	Timezone()
	{
	}

	Timezone(const TimeChangeRule& dstStart, const TimeChangeRule& stdStart)
	{
		init(dstStart, stdStart);
	}

	void init(const TimeChangeRule& dstStart, const TimeChangeRule& stdStart);

	/**
	 * @brief Convert the given UTC time to local time, standard or daylight time
	 * @param utc Time in UTC
	 * @param rule Optionally return the rule used to convert the time
	 * @retval time_t The local time
	 */
	time_t toLocal(time_t utc, const TimeChangeRule** rule = nullptr);

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
	 * @retval TimeChangeRule
	 */
	const TimeChangeRule& getRule(bool isDst) const
	{
		return isDst ? dstRule : stdRule;
	}

private:
	/*
	 * Calculate the DST and standard time change points for the given
	 * given year as local and UTC time_t values.
	 */
	void calcTimeChanges(unsigned yr);

private:
	TimeChangeRule dstRule = {"DST", First, Sun, Jan, 1, 0}; ///< rule for start of dst or summer time for any year
	TimeChangeRule stdRule = {"UTC", First, Sun, Jan, 1, 0}; ///< rule for start of standard time for any year
	time_t dstStartUTC = 0;									 ///< dst start for given/current year, given in UTC
	time_t stdStartUTC = 0;									 ///< std time start for given/current year, given in UTC
	time_t dstStartLoc = 0;									 ///< dst start for given/current year, given in local time
	time_t stdStartLoc = 0; ///< std time start for given/current year, given in local time
};
