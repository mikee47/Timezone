#include <SmingCore.h>
#include <ZonedClock.h>
#include <tzdata.h>
#include <WVector.h>

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put your SSID and password here
#define WIFI_PWD "PleaseEnterPass"
#endif

namespace
{
SimpleTimer printTimer;

DEFINE_FSTR_LOCAL(zoneNames, "Europe/London\0"
							 "Europe/Berlin\0"
							 "Europe/Moscow\0"
							 "Australia/Sydney\0"
							 "Asia/Qatar\0")

Vector<ZonedClock> clocks;

std::unique_ptr<NtpClient> ntpClient;

void printClock(const ZonedClock& clock)
{
	Serial << clock.getLocation() << ": " << clock.now().toString();
	if(clock.getTimezone().hasDaylightSavings()) {
		auto change = clock.getNextChange();
		Serial << _F(", Next Change ") << change.toString() << " (" << change.toUtc().toString() << ")" << endl;
	} else {
		Serial << _F(" (no daylight savings)") << endl;
	}
}

void onPrintSystemTime()
{
	Serial << _F("\0337"   // Save cursor position
				 "\033[H"  // Home
				 "\033[0r" // Set scrolling region
	);
	Serial << _F("UTC: ") << ZonedTime(SystemClock.now(eTZ_UTC)).toString() << endl;
	for(auto& clock : clocks) {
		printClock(clock);
	}
	Serial << _F("\033[10r" // Set scrolling region
				 "\0338"	// Restore cursor position
	);
}

void onNtpReceive([[maybe_unused]] NtpClient& client, time_t timestamp)
{
	SystemClock.setTime(timestamp, eTZ_UTC);
	Serial << _F("Time synchronized: ") << SystemClock.getSystemTimeString() << endl;
}

// Will be called when WiFi station timeout was reached
void connectFail([[maybe_unused]] const String& ssid, MacAddress, WifiDisconnectReason)
{
	Serial << _F("I'm NOT CONNECTED!") << endl;
}

void gotIP(IpAddress ip, [[maybe_unused]] IpAddress netmask, [[maybe_unused]] IpAddress gateway)
{
	Serial << _F("Connected as ") << ip << endl;

	ntpClient = std::make_unique<NtpClient>(onNtpReceive);
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true); // Allow debug print to serial

	Serial << _F("\033[H"   // Home
				 "\033[0J"  // Erase screen
				 "\033[10B" // Down 10 lines
				 )
		   << _F("Sming. Let's do smart things!") << endl;

	for(auto name : CStringArray(zoneNames)) {
		auto zone = TZ::findZone(name);
		if(!zone) {
			Serial << "Error! Zone '" << name << "' not found" << endl;
			continue;
		}
		clocks.add(*zone);
	}

	// Station - WiFi client
	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD); // Put your SSID and password here

	printTimer.initializeMs<1000>(onPrintSystemTime).start();

	WifiEvents.onStationDisconnect(connectFail);
	WifiEvents.onStationGotIP(gotIP);
}
