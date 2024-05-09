#include <SmingCore.h>
#include <Timezone.h>
#include "Tabulator.h"
#include "Menu.h"
#include <Timezone/CountryMap.h>
#include <Timezone/ZoneTable.h>
#include <Timezone/TzInfo.h>

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

void printTzInfo(const String& name)
{
	auto showRule = [](const char* name, TzData::Year from, TzData::Year to) {
		auto rules = std::make_unique<CsvTable<RuleRecord>>(new FileStream(F("rules/") + name), ' ', "", 64);
		for(auto rule : *rules) {
			if(rule.to() < from) {
				continue;
			}
			if(rule.from() > to) {
				break;
			}
			Serial << "    " << rule << endl;
			// Serial << "    " << rule.row.join(" ") << endl;
		}
	};

	String area;
	String location;
	int i = name.lastIndexOf('/');
	if(i < 0) {
		area = F("default");
		location = name;
	} else {
		area = name.substring(0, i);
		location = name.substring(i + 1);
	}
	auto zoneInfo = std::make_unique<CsvTable<TzInfoRecord>>(new FileStream(area + ".zi"), ' ', "", 64);
	TzData::Year yearFrom{};
	bool found{false};
	for(auto rec : *zoneInfo) {
		switch(rec.type()) {
		case TzInfoRecord::Type::zone: {
			if(found) {
				return;
			}
			ZoneRecord zone(rec);
			found = (location == zone.location());
			break;
		}
		case TzInfoRecord::Type::link: {
			LinkRecord link(rec);
			if(found) {
				return;
			}
			if(location == link.location()) {
				Serial << _F("Link ") << link.location() << " -> " << link.zone() << endl;
				return;
			}
			break;
		}
		case TzInfoRecord::Type::era: {
			if(!found) {
				break;
			}
			EraRecord era(rec);
			auto until = era.until();
			if(until) {
				Serial << "Until " << String(until);
			} else {
				Serial << "From " << yearFrom;
			}
			Serial << " stdoff " << String(era.stdoff());
			auto ruleKind = era.ruleKind();
			if(ruleKind == EraRecord::RuleKind::time) {
				TzData::TimeOffset time(era.rule());
				Serial << ", dstoff " << String(time);
			}
			Serial << endl << "  " << era.row.join(" ") << endl;
			if(ruleKind == EraRecord::RuleKind::rule) {
				showRule(era.rule(), yearFrom, until.year);
			}
			yearFrom = until.year + 1;
			break;
		}
		case TzInfoRecord::Type::invalid:
			break;
		}
	}
}

void verifyData()
{
	auto reftable = std::make_unique<CsvTable<TzInfoRecord>>(new FileStream("to2050.tzs"), '\t', "", 256);
	String zone;
	for(auto rec : *reftable) {
		if(rec.type() == TzInfoRecord::Type::link) {
			continue;
		}
		if(rec.row.startsWith("TZ")) {
			if(zone) {
				// break;
			}
			auto s = rec.row[0];
			s += 4;
			zone.setString(s, strlen(s) - 1);
			Serial << zone << endl;
			continue;
		}
		// Serial << rec.row.join(" ") << endl;
		TzsRecord tzs(rec);
		auto dt = tzs.datetime();
		if(dt) {
			Serial << "  " << tzs.datetime().format(_F("%d %b %Y %H:%M:%S")) << " " << String(tzs.interval()) << " "
				   << tzs.tag() << endl;
		}
		// Serial << String(tzs.time()) << endl;
	}
}

void showRootMenu();

void zoneSelected(String name)
{
	menu.begin(name);
	menu.additem(F("Make this the active zone"), [name]() {
		Serial << F("TODO") << endl;
		showRootMenu();
	});
	menu.additem(F("Show details"), [name]() {
		printTzInfo(name);
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

void selectCountry(String area)
{
	menu.begin(F("Countries in ") + Zone::getAreaCaption(area));

	Vector<Country::Code> codes;
	{
		auto zonetab = openZoneTable();
		for(auto zone : *zonetab) {
			if(!zone.areaIs(area)) {
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

void selectArea()
{
	menu.begin(F("Areas"));

	auto zonetab = openZoneTable();
	ZoneFilter filter(*zonetab, true);
	filter.match(nullptr, false);
	for(auto& s : filter.matches) {
		menu.additem(Zone::getAreaCaption(s), [name = s]() { selectCountry(name); });
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
}

void listCountriesByTimezone()
{
	// Create temporary hash map for faster country lookup
	CountryMap countries(*openCountryTable());

	// Get list of areas
	auto zonetab = openZoneTable();
	ZoneFilter areas(*zonetab, true);
	areas.match(nullptr, false);

	for(auto& area : areas.matches) {
		Serial << Zone::getAreaCaption(area) << endl;
		for(auto zone : *zonetab) {
			if(!zone.areaIs(area)) {
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
	menu.additem(F("Select by area"), selectArea);
	menu.additem(F("List timezones"), []() {
		listTimezones();
		showRootMenu();
	});
	menu.additem(F("List countries by timezone"), []() {
		listCountriesByTimezone();
		showRootMenu();
	});
	menu.additem(F("Verify vs. to2050.tzs"), []() {
		verifyData();
		showRootMenu();
	});
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
}
