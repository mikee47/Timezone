#include <SmingCore.h>
#include <Timezone.h>
#include "tzdata.h"
#include "tzindex.h"
#include "Tabulator.h"
#include "Menu.h"
#include <Timezone/CountryMap.h>
#include <Timezone/ZoneTable.h>

namespace
{
Menu menu(Serial);

std::unique_ptr<CountryTable> openCountryTable()
{
	return std::make_unique<CountryTable>("iso3166.tab");
}

std::unique_ptr<ZoneTable> openZoneTable()
{
	return std::make_unique<ZoneTable>("zone1970.tab");
}

String timeString(time_t time, const String& id)
{
	String s = DateTime(time).format(_F("%a, %d %b %Y %T"));
	s += ' ';
	s += id;
	return s;
}

void printCurrentTime()
{
	auto utc = SystemClock.now(eTZ_UTC);
	auto local = SystemClock.now(eTZ_Local);
	Serial << timeString(utc, "UTC") << endl;
	Serial << timeString(local, "???") << endl;
}

void showRootMenu();

void zoneSelected(String name)
{
	menu.begin(name);
	menu.additem(F("Make this the active zone"), [name]() {
		Serial << F("TODO") << endl;
		showRootMenu();
	});
	menu.additem(F("Main menu"), showRootMenu);
	menu.end();
}

void selectZone(Country::Code code, String name)
{
	menu.begin(F("Available timezones for ") + name);
	String codestr(code);
	auto zonetab = openZoneTable();
	for(auto zone : *zonetab) {
		if(zone.codes().contains(codestr)) {
			menu.additem(zone.caption(), [name = String(zone.name())]() { zoneSelected(name); });
		}
	}
	menu.end();
}

void selectCountry(String continent)
{
	menu.begin(F("Countries in ") + Zone::getContinentCaption(continent));

	Vector<Country::Code> codes;
	{
		auto zonetab = openZoneTable();
		for(auto zone : *zonetab) {
			if(!zone.continentIs(continent)) {
				continue;
			}
			for(auto code : zone.codes()) {
				if(!codes.contains(code)) {
					codes.add(code);
				}
			}
		}
	}

	auto countries = openCountryTable();
	for(auto country : *countries) {
		auto code = country.code();
		if(codes.contains(code)) {
			String name(country);
			menu.additem(name, [code, name]() { selectZone(code, name); });
		}
	}

	menu.end();
}

void selectContinent()
{
	menu.begin(F("Continents"));

	auto zonetab = openZoneTable();
	ZoneFilter filter(*zonetab, true);
	filter.match(nullptr, false);
	for(auto& s : filter.matches) {
		menu.additem(Zone::getContinentCaption(s), [name = s]() { selectCountry(name); });
	}

	menu.end();
}

void enterTimezone()
{
	auto completionCallback = [](String& line) -> void {
		auto zonetab = openZoneTable();
		ZoneFilter filter(*zonetab, true);
		switch(filter.match(line, true)) {
		case 0:
			break;
		case 1:
			line = filter[0];
			break;
		default:
			Serial.println();
			Tabulator tab(Serial);
			for(auto s : filter.matches) {
				tab.print(s);
			}
			tab.println();
			menu.prompt();
			Serial.print(line);
		}
	};

	auto submitCallback = [](String& line) -> void {
		// Accept abbreviated form if it matches exactly one timezone
		auto zonetab = openZoneTable();
		ZoneFilter filter(*zonetab, true);
		String match;
		unsigned pos{0};
		bool ambiguous{false};
		auto len = line.length();
		while(pos < len) {
			auto sep = line.indexOf('/', pos);
			if(sep < 0) {
				sep = len;
			}
			match += line.substring(pos, sep);
			auto count = filter.match(match, true);
			if(count == 1) {
				match = filter[0];
				pos = sep + 1;
				continue;
			}

			match = nullptr;
			ambiguous = (count > 1);
			break;
		}

		if(match && !line.endsWith('/')) {
			zoneSelected(match);
			return;
		}

		if(ambiguous) {
			Serial << line << _F(" is ambiguous") << endl;
		} else {
			Serial << F("Timezone '") << line << F("' not found!") << endl;
		}
		showRootMenu();
	};

	Serial << F("Use TAB for auto-completion.") << endl;
	menu.custom(F("Timezone: "), submitCallback, completionCallback);
}

void listTimezones()
{
	Serial << F("Timezone").padRight(40) << F("Caption") << endl;
	Serial << String().padRight(38, '-') << "  " << String().pad(38, '-') << endl;
	auto zonetab = openZoneTable();
	for(auto zone : *zonetab) {
		Serial << String(zone.name()).padRight(40) << zone.caption() << endl;
	}
	showRootMenu();
}

void listCountriesByTimezone()
{
	// Create temporary hash map for faster country lookup
	CountryMap countries(*openCountryTable());

	// Get list of continents
	auto zonetab = openZoneTable();
	ZoneFilter continents(*zonetab, true);
	continents.match(nullptr, false);

	for(auto& continent : continents.matches) {
		Serial << Zone::getContinentCaption(continent) << endl;
		for(auto zone : *zonetab) {
			if(!zone.continentIs(continent)) {
				continue;
			}
			Serial << "  " << zone.name() << ": ";

			unsigned matchCount{0};
			for(auto code : zone.codes()) {
				if(matchCount++) {
					Serial << ", ";
				}
				Serial << countries[code];
			}
			Serial << endl;
		}
	}

	showRootMenu();
}

void printFile(const String& filename)
{
	auto table = std::make_unique<CsvTable<CsvRecord>>(new FileStream(filename), '\t', "", 256);
	unsigned count{0};
	unsigned comments{0};
	size_t size{0};
	for(auto rec : *table) {
		++count;
		size += rec.row.length();
		if(rec.row.startsWith("#")) {
			++comments;
		}
		// Serial << rec[0] << endl;
	}
	Serial << filename << ": " << size << _F(" chars in ") << count << _F(" records, ") << comments << _F(" comments.")
		   << endl;
}

void showRootMenu()
{
	menu.begin(F("Main menu"));
	printCurrentTime();

	menu.additem(F("Enter timezone"), enterTimezone);
	menu.additem(F("Select by continent"), selectContinent);
	menu.additem(F("List timezones"), listTimezones);
	menu.additem(F("List countries by timezone"), listCountriesByTimezone);
	menu.additem(F("Scan all files"), []() {
		OneShotFastMs timer;
		Directory dir;
		if(dir.open(nullptr)) {
			while(dir.next()) {
				printFile(String(dir.stat().name));
			}
		}
		auto elapsed = timer.elapsedTime();
		Serial << _F("Scan took ") << elapsed.toString() << endl;
		showRootMenu();
	});
	menu.end();
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

	Serial.onDataReceived([](Stream& source, char, unsigned short) { menu.handleInput(source); });

	showRootMenu();
	menu.prompt();
	return;

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

	auto elapsed = timer.elapsedTime();
	Serial << "DONE " << elapsed << endl;
}
