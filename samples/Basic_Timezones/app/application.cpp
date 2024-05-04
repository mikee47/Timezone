#include <SmingCore.h>
#include <Timezone.h>
#include "tzdata.h"
#include "tzindex.h"
#include <Timezone/CountryMap.h>
#include <Timezone/ZoneTable.h>
#include <Data/CsvReader.h>
#include <WVector.h>

namespace
{
CountryMap countries;
ZoneTable timezones;

void printTimezones()
{
	countries.load("countries");
	timezones.load("timezones");

	// Get list of continents
	Vector<String> continents;
	while(auto zone = timezones.next()) {
		auto continent = zone.continent();
		if(continent && !continents.contains(continent)) {
			continents.add(continent);
		}
	}

	for(auto& continent : continents) {
		Serial << Zone::getContinentCaption(continent) << endl;
		timezones.reset();
		while(auto zone = timezones.next()) {
			if(!zone.continentIs(continent)) {
				continue;
			}
			Serial << "  " << zone.caption() << endl;

			for(auto code : zone.codes()) {
				Serial << "    " << countries[code] << endl;
			}
		}
	}

	// while(timezones.next()) {
	// 	auto cmt = timezones.getValue(cmtcol);
	// 	auto zone = timezones.getValue(zonecol);
	// 	Serial << zone << " - " << cmt << endl;
	// }
}

} // namespace

// Will be called when WiFi hardware and software initialization was finished
// And system initialization was completed

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true); // Allow debug print to serial
	Serial.println(_F("Sming. Let's do smart things!"));

	fwfs_mount();

#if 0
	auto check = [](const TzInfo& zone) {
		TZ::setZone(zone.rules);
		Serial << endl << zone.fullName() << ": " << _tzname[0] << ", " << _tzname[1] << ", " << zone.rules << endl;
		TZ::calcLimits(2024);

		// struct tm tm = {
		// 	.tm_hour = 21,
		// 	.tm_mday = 27,
		// 	.tm_mon = 4 - 1,
		// 	.tm_year = 2024 - 1900,
		// };
		// auto time = mktime(&tm);
		// Serial << "mktime " << DateTime(time).toHTTPDate() << endl;

		auto& tzinfo = TZ::getInfo();
		Serial << "tzrule[0] " << DateTime(tzinfo.__tzrule[0].change).toHTTPDate() << endl;
		Serial << "tzrule[1] " << DateTime(tzinfo.__tzrule[1].change).toHTTPDate() << endl;
	};

	// These two have invalid POSIX strings: time is negative
	check(TZ::America::Scoresbysund);
	check(TZ::America::Nuuk);

	// check(TZ::Europe::London);
	// check(TZ::Europe::Berlin);
	// check(TZ::Etc::GMT_N10);
	// check(TZ::CET);
	// check(TZ::Factory);
	// check(TZ::Asia::Gaza);
	// check(TZ::GMT);
	// check(TZ::Canada::Pacific);
	// check(TZ::Pacific::Chatham);
	// check(TZ::Asia::Famagusta);

#endif

#if 0

	// for(auto& country : *TZ::Index::europe.countries) {
	// 	Serial << *country.name << endl;
	// 	for(auto& tz : *country.timezones) {
	// 		Serial << "  " << tz.caption() << endl;
	// 	}
	// }

	for(auto& continent : TZ::Index::continents) {
		Serial << continent.caption() << endl;
		for(auto& country : *continent.countries) {
			Serial << "  " << *country.name << endl;
			for(auto& tz : *country.timezones) {
				Serial << "    " << tz.caption() << endl;
			}
		}
	}
#endif

	OneShotFastMs timer;

	printTimezones();

	auto elapsed = timer.elapsedTime();
	Serial << "DONE " << elapsed << endl;
}
