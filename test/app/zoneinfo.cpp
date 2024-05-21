#include "Common.h"

#define VERBOSE (DEBUG_VERBOSE_LEVEL == DBG)

namespace
{
/*
 * MinGW/Windows uses non-posix timezone strings so none of this will work.
 */
#ifndef __WIN32
[[maybe_unused]] ZonedTime getLocalTime(const String& tzstr, time_t utc)
{
	// Set libc timezone for checking
	setenv("TZ", tzstr.c_str(), 1);
	tzset();

	struct tm* t = localtime(&utc);
	DateTime dt;
	dt.setTime(t->tm_sec, t->tm_min, t->tm_hour, t->tm_mday, t->tm_mon, t->tm_year + 1900);
	time_t local = dt;

	DateTime::ZoneInfo zoneInfo{
		.tag = DateTime::ZoneInfo::Tag::fromString(tzname[t->tm_isdst]),
		.offsetMins = int16_t((local - utc) / 60),
		.isDst = bool(t->tm_isdst),
	};

	return ZonedTime(utc, zoneInfo);
}
#endif

} // namespace

class ZoneinfoTest : public TestGroup
{
public:
	ZoneinfoTest() : TestGroup(_F("Zoneinfo"))
	{
	}

	void execute() override
	{
		DateTime dt;
		dt.fromISO8601(F("2024-05-01"));
		time_t refTime = dt;

		utcNow = SystemClock.now(eTZ_UTC);
		if(utcNow < refTime) {
			Serial << _F("Clock not set, using ref time") << endl;
			utcNow = refTime;
		}
		Serial << _F("Now: ") << toString(utcNow) << endl << endl;
		year = DateTime(utcNow).Year;

		unsigned zoneCount{0};
		for(const TZ::ZoneList& zones : TZ::areas) {
			zoneCount += zones.length();
		}
		Serial << _F("Database source ") << _F(ZONEINFO_SOURCE) << endl
			   << "Version " << _F(ZONEINFO_VERSION) << " / " << ZONEINFO_VER_MAJOR << '.' << ZONEINFO_VER_MINOR
			   << " / 0x" << String(ZONEINFO_VER, HEX) << endl
			   << _F("Found ") << zoneCount << _F(" zones in database") << endl;

		itarea = TZ::areas.begin();
		pending();
		checkArea();
	}

	void checkArea()
	{
		for(auto& zone : (*itarea).content()) {
			Timezone tz(zone);
			CHECK(tz);

			Serial << zone.name().pad(35);
#if TZINFO_WANT_TZSTR
			Serial << String(zone.tzstr).pad(48);
#endif
			Serial << tz << endl;

			CHECK_EQ(tz.toString(), zone.tzstr);

			auto checkLocal = [&](time_t local) {
				auto utc = tz.toUTC(local);
				auto time = tz.makeZoned(utc);
				CHECK_EQ(time.local(), local);
				bool valid = verifyTime(zone, utc, 0, time);
				if(!valid || VERBOSE) {
					Serial << _F("         UTC: ") << toString(utc.toUtc()) << endl;
					Serial << _F("         LCL: ") << toString(time) << endl;
				}
			};

			// Start of year
			DateTime dt;
			dt.setTime(0, 0, 0, 1, dtJanuary, year + 1);
			checkLocal(dt);

			// End of year
			dt.setTime(59, 59, 23, 31, dtDecember, year + 1);
			checkLocal(dt);

			/*
			 * For zones without daylight savings, verify:
			 *   std offset
			 * 	 behaviour of 'next change time' (should be infinite)
			 */
			if(!tz.hasDaylightSavings()) {
				// No DST, fixed offset
				auto tt = tz.getTransition(year, false);
				CHECK(tt == TZ::maxTime);
				tt = tz.getTransition(year, true);
				CHECK(tt == TZ::maxTime);

				auto time = tz.makeZoned(utcNow);
				bool valid = verifyTime(zone, utcNow, 0, time);
				if(!valid || VERBOSE) {
					Serial << _F("         STD: ") << toString(time) << endl;
				}
				continue;
			}

			/*
			 * For zones with DST, verify:
			 *   std offset and next change time
			 *   dst offset and next change time
			 */

			auto printTransition = [&](const ZonedTime& transitionTime) {
				bool toDst = transitionTime.isDst();
				const char* tags[] = {"STD", "DST"};
				ZonedTime tFrom = tz.makeZoned(transitionTime, true);

				CHECK(tFrom.isDst() == !toDst);
				bool valid = verifyTime(zone, transitionTime, -1, tFrom);
				if(!valid || VERBOSE) {
					Serial << _F("    from ") << tags[!toDst] << ": " << toString(tFrom) << endl;
				}

				ZonedTime tTo = tz.makeZoned(transitionTime);
				CHECK(tTo.isDst() == toDst);
				valid = verifyTime(zone, transitionTime, 0, tTo);
				if(!valid || VERBOSE) {
					Serial << _F("  Change:     ") << toString(transitionTime) << endl;
					Serial << _F("      to ") << tags[toDst] << ": " << toString(tTo) << endl;
				}

#if TZINFO_WANT_TRANSITIONS
				// Find closest match in transition table
				TZ::Transition ttMatch{TZ::invalidTime};
				for(auto t : zone.transitions) {
					if(t < transitionTime) {
						if(toDst == t.isdst) {
							ttMatch = t;
						}
						continue;
					}
					if(t > transitionTime) {
						if(toDst == t.isdst) {
							ttMatch = t;
						}
						break;
					}
					ttMatch = t;
					break;
				}

				if(ttMatch == transitionTime) {
					return;
				}
				if(!VERBOSE) {
					Serial << _F("  Change on:  ") << toString(tFrom.toUtc()) << endl;
					Serial << _F("              ") << toString(tFrom) << endl;
					Serial << _F("      to ") << tags[toDst] << ": " << toString(tTo) << endl;
				}
				if(ttMatch == TZ::invalidTime) {
					Serial << _F("!!   Transition not found") << endl;
				} else {
					DateTime::ZoneInfo zi;
					strncpy(zi.tag.value, &zone.tznames[ttMatch.desigidx], zi.tag.maxSize + 1);
					zi.offsetMins = ttMatch.offsetMins;
					zi.isDst = ttMatch.isdst;
					ZonedTime zm(ttMatch, zi);
					Serial << _F("!! Actual on: ") << toString(ttMatch) << endl;
					Serial << _F("!!        to: ") << toString(zm) << endl;
				}
#endif
			};

			auto tt = tz.getNextChange(utcNow);
			printTransition(tt);
			tt = tz.getNextChange(tt);
			printTransition(tt);
		}

		++itarea;
		if(itarea == TZ::areas.end()) {
			complete();
		} else {
			System.queueCallback([this]() { checkArea(); });
		}
	}

	/**
	 * @brief Verify against libc times
	 * @param zone
	 * @param utc The UTC timestamp to convert to local time
	 * @param offset An offset in seconds to apply to UTC for checkout around transition times
	 * @param time The expected local time
	 */
	bool verifyTime(const TZ::Info& zone, time_t utc, int offset, const ZonedTime& time)
	{
#if defined(ARCH_HOST) && !defined(__WIN32)
#if TZINFO_WANT_TZSTR
		bool tzstrTimeOk;
		{
			auto localTime = getLocalTime(zone.tzstr, utc + offset);
			localTime = ZonedTime(utc, localTime.getZoneInfo());
			tzstrTimeOk = (time.local() == localTime.local());
			if(!tzstrTimeOk) {
				Serial << _F("!! Doesn't match localtime()") << endl;
				Serial << _F("offset mins = ") << time.offsetMins() << endl;
				Serial << _F("  check: ") << toString(localTime) << endl;
				CHECK(tzstrTimeOk);
			}
		}
#endif
#ifdef ARCH_HOST
		// Query the full database
		{
			auto localTime = getLocalTime(zone.name(), utc + offset);
			localTime = ZonedTime(localTime - offset, localTime.getZoneInfo());
			if(time != localTime) {
				Serial << _F("!! Doesn't match localtime() using ") << zone.name() << endl;
				Serial << _F("      check: ") << toString(localTime);
#if TZINFO_WANT_TZSTR
				if(tzstrTimeOk) {
					Serial << _F(" [POSIX string is wrong]");
				}
#endif
				Serial.println();
				return false;
			}
		}
#endif
#endif
		return true;
	}

private:
	TZ::AreaMap::Iterator itarea;
	time_t utcNow;
	uint16_t year;
};

void REGISTER_TEST(zoneinfo)
{
	registerGroup<ZoneinfoTest>();
}
