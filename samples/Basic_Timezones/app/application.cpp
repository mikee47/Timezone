#include <SmingCore.h>
#include <Timezone.h>
#include "tzdata.h"
#include "tzindex.h"
#include <Timezone/CountryMap.h>
#include <Timezone/ZoneTable.h>
#include <Data/CsvReader.h>
#include <WVector.h>
#include <Data/Buffer/LineBuffer.h>

namespace
{
DEFINE_FSTR(commandPrompt, "> ");

CountryMap countries;
ZoneTable timezones;

namespace Menu
{
using Callback = Delegate<void()>;
Vector<Callback> callbacks;
unsigned column;

void init(const String& caption)
{
	Serial << endl << caption << ":" << endl;
	column = 0;
	callbacks.clear();
}

void additem(const String& caption, Callback callback)
{
	constexpr unsigned colWidth{25};
	constexpr unsigned maxLineWidth{100};

	auto choice = callbacks.count() + 1;

	String s;
	s.reserve(colWidth);
	s += "  ";
	s += choice;
	s += ") ";
	s += caption;

	unsigned startColumn{column};
	if(column % colWidth) {
		auto count = (column + colWidth - 1) / colWidth;
		startColumn = count * colWidth;
	}

	if(startColumn + s.length() > maxLineWidth) {
		column = 0;
		Serial.println();
	} else {
		s = s.padLeft(s.length() + startColumn - column);
	}

	Serial << s;
	column += s.length();

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

void ready()
{
	Serial.println();
	if(callbacks.count() == 1) {
		Serial << '1' << endl;
		select(1);
	} else {
		Serial << _F("Please select an option (1 - ") << callbacks.count() << ")" << endl;
	}
}

void prompt()
{
	Serial.print(commandPrompt);
}

}; // namespace Menu

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

void selectZone(uint16_t code)
{
	Menu::init(F("Available timezones for ") + countries[code]);
	String codestr = CountryMap::getCodeString(code);
	timezones.reset();
	while(auto zone = timezones.next()) {
		if(zone.codes().contains(codestr)) {
			Menu::additem(zone.caption(), [name = String(zone.name())]() { zoneSelected(name); });
		}
	}
	Menu::ready();
}

void selectCountry(String continent)
{
	Menu::init(F("Countries in ") + Zone::getContinentCaption(continent));

	Vector<uint16_t> codes;
	timezones.reset();
	while(auto zone = timezones.next()) {
		if(!zone.continentIs(continent)) {
			continue;
		}
		for(auto s : zone.codes()) {
			auto code = CountryMap::makeCode(s);
			if(!codes.contains(code)) {
				codes.add(code);
			}
		}
	}

	for(auto c : countries) {
		auto code = c.key();
		if(codes.contains(code)) {
			Menu::additem(c.value(), [code]() { selectZone(code); });
		}
	}

	Menu::ready();
}

void selectContinent()
{
	Menu::init(F("Continents"));

	Vector<String> continents;
	timezones.reset();
	while(auto zone = timezones.next()) {
		auto s = zone.continent();
		if(s && !continents.contains(s)) {
			continents.add(s);
		}
	}

	for(auto& s : continents) {
		Menu::additem(Zone::getContinentCaption(s), [name = s]() { selectCountry(name); });
	}

	Menu::ready();
}

void listTimezones()
{
	Serial << F("Timezone").padRight(40) << F("Caption") << endl;
	Serial << String().padRight(38, '-') << "  " << String().pad(38, '-') << endl;
	timezones.reset();
	while(auto zone = timezones.next()) {
		Serial << String(zone.name()).padRight(40) << zone.caption() << endl;
	}
	showRootMenu();
}

void showRootMenu()
{
	Menu::init(F("Main menu"));
	Menu::additem(F("Select continent"), selectContinent);
	Menu::additem(F("List timezones"), listTimezones);
	Menu::ready();
}

void printTimezones()
{
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

void serialReceive(Stream& source, char, unsigned short)
{
	static LineBuffer<32> buffer;

	using Action = LineBufferBase::Action;
	switch(buffer.process(source, Serial)) {
	case Action::submit: {
		if(!buffer) {
			break;
		}
		auto choice = String(buffer).toInt();
		buffer.clear();
		Menu::select(choice);
		break;
	}

	case Action::clear:
		break;

	default:;
		return;
	}

	Serial.print(commandPrompt);
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

	countries.load("countries");
	timezones.load("timezones");

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

	printTimezones();

	auto elapsed = timer.elapsedTime();
	Serial << "DONE " << elapsed << endl;
}
