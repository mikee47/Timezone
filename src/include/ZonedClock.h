/****
 * ZonedClock.h
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
 ****/

#pragma once

#include "Timezone.h"
#include <SystemClock.h>

/**
 * @brief Maintain clock for specific timezone
 *
 * This class maintains local time for a specific timezone.
 * Time is referenced to the UTC time obtained from `SystemClock`.
 *
 * Applications call the `now()` method to obtain the local time.
 * The 'next transition' time is tracked (in UTC) so that transition between standard
 * and daylight savings time is handled smoothly.
 *
 */
class ZonedClock
{
public:
	/**
	 * @brief Construct a clock from timezone information, plus identifier
	 * @param timezone
	 * @param location For identification purposes
	 */
	ZonedClock(const Timezone& timezone, const String& location) : timezone(timezone), location(location)
	{
	}

	/**
	 * @brief Construct a clock given a definition from tzdb.
	 */
	template <typename ZoneDef> ZonedClock(const ZoneDef& zonedef) : ZonedClock(zonedef, zonedef.name())
	{
	}

	String getLocation() const
	{
		return location.c_str();
	}

	bool operator==(const String& name) const
	{
		return location == name;
	}

	/**
	 * @brief Get the current date and time
     */
	ZonedTime now() const
	{
		auto utc = SystemClock.now(eTZ_UTC);

		if(nextChange == TZ::invalidTime) {
			zoneInfo = timezone.makeZoned(utc).getZoneInfo();
			nextChange = timezone.getNextChange(utc);
		} else if(utc >= nextChange) {
			zoneInfo = nextChange.getZoneInfo();
			nextChange = timezone.getNextChange(utc);
		}

		return {utc, zoneInfo};
	}

	Timezone& getTimezone() const
	{
		return timezone;
	}

	/**
	 * @brief Get current time zone information
	 */
	const DateTime::ZoneInfo& getZoneInfo() const
	{
		return zoneInfo;
	}

	/**
	 * @brief Get time when next transition to/from daylight savings is due
	 */
	const ZonedTime getNextChange() const
	{
		return nextChange;
	}

private:
	mutable Timezone timezone;
	CString location;
	mutable ZonedTime nextChange{TZ::invalidTime};
	mutable DateTime::ZoneInfo zoneInfo;
};
