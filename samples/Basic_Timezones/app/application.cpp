#include <SmingCore.h>
#include <Timezone.h>
#include "tzdata.h"
#include "tzindex.h"
#include "Tabulator.h"
#include <Timezone/CountryMap.h>
#include <Timezone/ZoneTable.h>
#include <Data/Buffer/LineBuffer.h>

namespace
{
std::unique_ptr<CountryTable> openCountryTable()
{
	return std::make_unique<CountryTable>("iso3166.tab");
}

std::unique_ptr<ZoneTable> openZoneTable()
{
	return std::make_unique<ZoneTable>("zone1970.tab");
}

namespace Menu
{
using Callback = Delegate<void()>;
using LineCallback = Delegate<void(String& line)>;

Vector<Callback> callbacks;
LineCallback autoCompleteCallback;
LineCallback submitCallback;
Tabulator tabulator(Serial);
String commandPrompt;

void init(const String& caption)
{
	Serial << endl << caption << ":" << endl;
	callbacks.clear();
	autoCompleteCallback = nullptr;
	submitCallback = nullptr;
	commandPrompt = "> ";
}

void additem(const String& caption, Callback callback)
{
	auto choice = callbacks.count() + 1;

	String s;
	s += "  ";
	s += choice;
	s += ") ";
	s += caption;

	tabulator.print(s);

	callbacks.add(callback);
}

void select(uint8_t choice)
{
	unsigned index = choice - 1;
	if(index >= callbacks.count()) {
		Serial << "Invalid choice '" << choice << "'" << endl;
		return;
	}
	callbacks[index]();
}

void prompt()
{
	Serial.print(commandPrompt);
}

void submit(String& line)
{
	if(submitCallback) {
		submitCallback(line);
	} else if(line) {
		auto choice = line.toInt();
		Menu::select(choice);
	}
	prompt();
}

void ready()
{
	tabulator.println();
	if(callbacks.count() == 1) {
		Serial << '1' << endl;
		select(1);
	} else {
		Serial << _F("Please select an option (1 - ") << callbacks.count() << ")" << endl;
	}
}

}; // namespace Menu

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
	Menu::init(name);
	Menu::additem(F("Make this the active zone"), [name]() {
		Serial << F("TODO") << endl;
		showRootMenu();
	});
	Menu::additem(F("Main menu"), showRootMenu);
	Menu::ready();
}

void selectZone(Country::Code code, String name)
{
	Menu::init(F("Available timezones for ") + name);
	String codestr(code);
	auto zonetab = openZoneTable();
	for(auto zone : *zonetab) {
		if(zone.codes().contains(codestr)) {
			Menu::additem(zone.caption(), [name = String(zone.name())]() { zoneSelected(name); });
		}
	}
	Menu::ready();
}

void selectCountry(String continent)
{
	Menu::init(F("Countries in ") + Zone::getContinentCaption(continent));

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
			Menu::additem(name, [code, name]() { selectZone(code, name); });
		}
	}

	Menu::ready();
}

void selectContinent()
{
	Menu::init(F("Continents"));

	auto zonetab = openZoneTable();
	ZoneFilter filter(*zonetab, true);
	filter.match(nullptr, false);
	for(auto& s : filter.matches) {
		Menu::additem(Zone::getContinentCaption(s), [name = s]() { selectCountry(name); });
	}

	Menu::ready();
}

void enterTimezone()
{
	Menu::autoCompleteCallback = [](String& line) -> void {
		auto zonetab = openZoneTable();
		ZoneFilter filter(*zonetab, true);
		switch(filter.match(line, true)) {
		case 0:
			return;
		case 1:
			line = filter[0];
			return;
		default:
			Serial.println();
			for(auto s : filter.matches) {
				Menu::tabulator.print(s);
			}
			Menu::tabulator.println();
			Menu::prompt();
			Serial.print(line);
		}
	};

	Menu::submitCallback = [](String& line) -> void {
		auto zonetab = openZoneTable();
		for(auto zone : *zonetab) {
			auto name = zone.name();
			if(line.equalsIgnoreCase(name)) {
				zoneSelected(name);
				return;
			}
		}

		Serial << F("Timezone '") << line << F("' not found!") << endl;
		showRootMenu();
	};

	Serial << F("Use TAB for auto-completion.") << endl;
	Menu::commandPrompt = F("Timezone: ");
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
	Menu::init(F("Main menu"));
	printCurrentTime();

	Menu::additem(F("Enter timezone"), enterTimezone);
	Menu::additem(F("Select continent"), selectContinent);
	Menu::additem(F("List timezones"), listTimezones);
	Menu::additem(F("List countries by timezone"), listCountriesByTimezone);
	Menu::additem(F("Scan all files"), []() {
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
	Menu::ready();
}

void serialReceive(Stream& source, char, unsigned short)
{
	static LineBuffer<32> buffer;

	using Action = LineBufferBase::Action;

	int c;
	while((c = source.read()) >= 0) {
		if(c == '\t' && Menu::autoCompleteCallback) {
			String line(buffer);
			Menu::autoCompleteCallback(line);
			auto len = buffer.getLength();
			if(line.equals(buffer.getBuffer(), len)) {
				continue;
			}
			while(len--) {
				buffer.processKey('\b', &Serial);
			}
			for(auto c : line) {
				buffer.processKey(c, &Serial);
			}
			continue;
		}
		switch(buffer.processKey(c, &Serial)) {
		case Action::submit: {
			String line(buffer);
			buffer.clear();
			Menu::submit(line);
			break;
		}

		case Action::clear:
			Menu::prompt();
			break;

		case Action::echo:
		case Action::backspace:
		case Action::none:
			break;
		}
	}
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

	Serial.onDataReceived(serialReceive);
	showRootMenu();
	Menu::prompt();
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
