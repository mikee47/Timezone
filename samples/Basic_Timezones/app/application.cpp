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
CountryMap countries;
ZoneTable timezones;

class Tabulator
{
public:
	Tabulator(Print& output, unsigned colWidth = 25, unsigned lineWidth = 100)
		: output(output), colWidth(colWidth), lineWidth(lineWidth)
	{
	}

	void print(const String& text)
	{
		unsigned startColumn{column};
		if(column % colWidth) {
			auto count = (column + colWidth - 1) / colWidth;
			startColumn = count * colWidth;
		}

		auto textlen = text.length();
		if(startColumn + textlen > lineWidth) {
			column = 0;
			output.println();
		} else {
			auto padlen = startColumn - column;
			output.print(String().pad(padlen));
			textlen += padlen;
		}
		output.print(text);
		column += textlen;
	}

	void println()
	{
		output.println();
		column = 0;
	}

private:
	Print& output;
	unsigned colWidth;
	unsigned lineWidth;
	unsigned column{0};
};

namespace Menu
{
DEFINE_FSTR(commandPrompt, "> ");

using Callback = Delegate<void()>;
using LineCallback = Delegate<void(String& line)>;

Vector<Callback> callbacks;
LineCallback autoCompleteCallback;
LineCallback submitCallback;
Tabulator tabulator(Serial);

void init(const String& caption)
{
	Serial << endl << caption << ":" << endl;
	callbacks.clear();
	autoCompleteCallback = nullptr;
	submitCallback = nullptr;
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

void listContinents(Vector<String>& list)
{
	list.clear();
	timezones.reset();
	while(auto zone = timezones.next()) {
		auto s = zone.continent();
		if(s && !list.contains(s)) {
			list.add(s);
		}
	}
}

void selectContinent()
{
	Menu::init(F("Continents"));

	Vector<String> continents;
	listContinents(continents);
	for(auto& s : continents) {
		Menu::additem(Zone::getContinentCaption(s), [name = s]() { selectCountry(name); });
	}

	Menu::ready();
}

void enterTimezone()
{
	Menu::autoCompleteCallback = [](String& line) -> void {
		unsigned matchCount{0};
		String match;
		const auto linelen = line.length();
		if(line.indexOf('/') >= 0) {
			timezones.reset();
			while(auto zone = timezones.next()) {
				auto name = zone.name();
				auto namelen = strlen(name);
				if(namelen < linelen) {
					continue;
				}
				if(!line.equalsIgnoreCase(name, linelen)) {
					continue;
				}
				if(matchCount == 0) {
					Serial.println();
					match = name;
				}
				Menu::tabulator.print(name);
				++matchCount;
			}
		} else {
			Vector<String> continents;
			listContinents(continents);
			Serial.println();
			for(auto& name : continents) {
				auto namelen = name.length();
				if(namelen < linelen) {
					continue;
				}
				if(!line.equalsIgnoreCase(name.c_str(), linelen)) {
					continue;
				}
				if(matchCount == 0) {
					Serial.println();
					match = name + '/';
				}
				Menu::tabulator.print(name + '/');
				++matchCount;
			}
		}

		if(matchCount) {
			Menu::tabulator.println();
			Menu::prompt();
			Serial.print(line);
			if(matchCount == 1) {
				line = match;
			}
		}
	};
	Menu::submitCallback = [](String& line) -> void {
		String timezone;
		timezones.reset();
		while(auto zone = timezones.next()) {
			auto name = zone.name();
			if(line.equalsIgnoreCase(name)) {
				timezone = name;
				break;
			}
		}

		if(timezone) {
			Serial << F("You selected '") << timezone << "'" << endl;
			// TODO: Do something with this, i.e. submenu
		} else {
			Serial << F("Timezone '") << line << F("' not found!") << endl;
		}

		showRootMenu();
	};

	Serial << F("Enter timezone");
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
	printCurrentTime();

	Menu::additem(F("Select continent"), selectContinent);
	Menu::additem(F("List timezones"), listTimezones);
	Menu::additem(F("Enter timezone"), enterTimezone);
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
