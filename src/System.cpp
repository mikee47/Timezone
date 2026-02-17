/****
 * System.cpp
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

#include "include/Timezone.h"
#include <SystemClock.h>

namespace TZ
{
namespace System
{
Timezone timezone;
ZonedTime nextChange{TZ::invalidTime};
DateTime::ZoneInfo zoneInfo;

void checkTimeZoneOffset(time_t systemTime)
{
	if(nextChange == TZ::invalidTime) {
		nextChange = timezone.makeZoned(systemTime);
	} else if(systemTime < nextChange) {
		return;
	}

	SystemClock.setTimeZone(nextChange.getZoneInfo());
	nextChange = timezone.getNextChange(systemTime);
}

void setTimezone(const Timezone& timezone)
{
	System::timezone = timezone;
	SystemClock.onCheckTimeZoneOffset(timezone.hasDaylightSavings() ? checkTimeZoneOffset : nullptr);
	nextChange = ZonedTime(TZ::invalidTime);
	checkTimeZoneOffset(SystemClock.now(eTZ_UTC));
}

const Timezone& getTimezone()
{
	return timezone;
}

const ZonedTime& getNextChange()
{
	return nextChange;
}

} // namespace System

} // namespace TZ
