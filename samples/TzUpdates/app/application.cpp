#include <SmingCore.h>
#include <Data/CsvParser.h>

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put your SSID and password here
#define WIFI_PWD "PleaseEnterPass"
#endif

namespace
{
// URL where IANA publish the current data files
DEFINE_FSTR(TZDB_URL, "http://data.iana.org/time-zones/tzdb/")

HttpClient downloadClient;
size_t totalRowSize;

bool handleRow(const CsvParser& parser, const CStringArray& row)
{
	String s = row.join("\t");
	Serial << s << endl;
	totalRowSize += s.length() + 1; // LF
	return true;
}

CsvParser parser(handleRow, '\t', "", 256);

int onRequestBody(HttpConnection& client, const char* at, size_t length)
{
	// m_printHex("HT", at, length);
	parser.parse(at, length);
	return 0;
}

int onDownload(HttpConnection& connection, bool success)
{
	parser.parse(nullptr, 0);
	Serial << _F("Bytes received ") << parser.tell() << _F(", output ") << totalRowSize << endl;
	auto status = connection.getResponse()->code;
	Serial << _F("Got response code: ") << unsigned(status) << " (" << status << _F("), success: ") << success << endl;
	return 0;
}

void gotIP(IpAddress ip, IpAddress netmask, IpAddress gateway)
{
	Serial << _F("Connected. Got IP: ") << ip << endl;

	auto request = new HttpRequest(String(TZDB_URL) + "europe");
	request->onBody(onRequestBody);
	request->onRequestComplete(onDownload);
	downloadClient.send(request);
}

void connectFail(const String& ssid, MacAddress bssid, WifiDisconnectReason reason)
{
	Serial.println(F("I'm NOT CONNECTED!"));
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true); // Allow debug print to serial
	Serial.println(F("Ready for SSL tests"));

	// Setup the WIFI connection
	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD); // Put your SSID and password here

	WifiEvents.onStationGotIP(gotIP);
	WifiEvents.onStationDisconnect(connectFail);
}
