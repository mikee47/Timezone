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
 * For each application-defined timezone we should keep track of zone changes.
 * These happen only infrequently so polling is quite inefficient.
 * Instead, we can track when the next change is and just do a comparison.
 *
 * Applications should not be restricted to just one timezone, even though
 * SystemClockClass only supports one local timezone.
 * Crucially, this `Timezone` library is an add-on so we don't want a dependency on it.
 *
 * Therefore, this class can be used to provide multiple local clocks all based on
 * the systemclock UTC.
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

	/**
	 * @brief Set the system clock's time
	 *
	 * TODO: If we already have a ZonedTime, why can't we just call SystemClock ?
     */
	bool setTime(ZonedTime time)
	{
		return SystemClock.setTime(time, eTZ_UTC);
	}

	/**
	 * @brief  Get current time as a string
     * @param  timeType Time zone to present time as, i.e. return local or UTC time
     * @retval String Current time in format: `dd.mm.yy hh:mm:ss`
     */
	String toString() const
	{
		return now().toString();
	}

	Timezone& getTimezone() const
	{
		return timezone;
	}

	/**
	 * @brief Get current time zone information
     * @retval ZoneInfo
	 */
	const DateTime::ZoneInfo& getZoneInfo() const
	{
		return zoneInfo;
	}

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
