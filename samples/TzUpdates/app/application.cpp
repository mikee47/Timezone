#include <SmingCore.h>
#include <CSV/Parser.h>
#include <Timezone/TzInfo.h>
#include <LittleFS.h>
#include <IFS/Debug.h>
#include <malloc_count.h>

#ifdef ARCH_HOST
#include <IFS/Host/FileSystem.h>
#endif

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put your SSID and password here
#define WIFI_PWD "PleaseEnterPass"
#endif

namespace
{
// URL where IANA publish the current data files
#define TZDB_URL "http://data.iana.org/time-zones/tzdb/"

// backward has all the links in it so put that first
#define TZDB_FILE_MAP(XX)                                                                                              \
	XX(backward)                                                                                                       \
	XX(africa)                                                                                                         \
	XX(antarctica)                                                                                                     \
	XX(asia)                                                                                                           \
	XX(australasia)                                                                                                    \
	XX(etcetera)                                                                                                       \
	XX(europe)                                                                                                         \
	XX(factory)                                                                                                        \
	XX(northamerica)                                                                                                   \
	XX(southamerica)

#define XX(name) #name "\0"
DEFINE_FSTR(TZDB_FILE_LIST, TZDB_FILE_MAP(XX))

HttpClient downloadClient;
unsigned fileIndex;
size_t totalRowSize;
String areaName;
File areaFile;
String ruleName;
File ruleFile;

bool handleRow(const CStringArray& row)
{
	auto type = row[0];
	if(!type) {
		// Probably a comment row
		return true;
	}

	switch(*type) {
	case 'Z': {
		CStringArray tmp = row;
		tmp.popFront();
		String location = tmp.popFront();

		String area = ZoneData::splitName(location);
		if(area != areaName) {
			String filename = F("updates/") + area + ".zi";
			if(!areaFile.open(filename, File::Create | File::Append | File::WriteOnly)) {
				Serial << filename << ": " << areaFile.getLastErrorString() << endl;
				break;
			}
			Serial << F("Created ") << filename << endl;
			areaName = area;
		}
		String s;
		s += "Z\t";
		s += location;
		s += '\n';
		areaFile.write(s);

		// Initial era
		s = tmp.join("\t");
		s += '\n';
		areaFile.write(s);
		totalRowSize += s.length();
		break;
	}

	case 'R': {
		// Omit type and name fields from rules since we store them in their own file
		CStringArray tmp = row;
		CStringArray newrow;
		newrow += "";
		tmp.popFront();
		newrow += "";
		auto name = tmp.popFront();
		while(auto cell = tmp.popFront()) {
			newrow.add(cell);
		}
		String s = newrow.join("\t");
		s += '\n';
		if(ruleName != name) {
			String filename = F("updates/rules/") + name;
			if(!ruleFile.open(filename, File::Create | File::Append | File::WriteOnly)) {
				Serial << filename << ": " << ruleFile.getLastErrorString() << endl;
				break;
			}
			Serial << F("Created ") << filename << endl;
			ruleName = name;
		}
		ruleFile.write(s);
		totalRowSize += s.length();
		break;
	}

	case 'L': {
		String link = row[2];
		String area = ZoneData::splitName(link);
		if(area != areaName) {
			String filename = F("updates/") + area + ".zi";
			if(!areaFile.open(filename, File::Create | File::Append | File::WriteOnly)) {
				Serial << filename << ": " << areaFile.getLastErrorString() << endl;
				break;
			}
			areaName = area;
		}
		String s;
		s += "L\t";
		s += row[1];
		s += "\t";
		s += link;
		s += '\n';
		areaFile.write(s);
		totalRowSize += s.length();
		break;
	}

	default:
		if(!areaFile) {
			Serial << F("ERROR! Area file not open.") << endl;
			break;
		}
		String s = row.join("\t");
		s += '\n';
		areaFile.write(s);
		totalRowSize += s.length();
	}

	return true;
}

CSV::Parser parser({
	.commentChars = "#",
	.lineLength = 256,
	.fieldSeparator = '\0',
});

int onRequestBody(HttpConnection&, const char* at, size_t length)
{
	size_t offset{0};
	while(parser.push(at, length, offset)) {
		handleRow(parser.getRow());
	}
	return 0;
}

void endParse()
{
	if(parser.flush()) {
		handleRow(parser.getRow());
	}
	areaFile.close();
	areaName = nullptr;
	ruleFile.close();
	ruleName = nullptr;
}

void requestNextFile();

int onDownload(HttpConnection& connection, bool success)
{
	endParse();
	Serial << _F("Bytes received ") << parser.tell() << _F(", output ") << totalRowSize << endl;
	auto status = connection.getResponse()->code;
	Serial << _F("Got response code: ") << unsigned(status) << " (" << status << _F("), success: ") << success << endl;

	requestNextFile();

	return 0;
}

void requestNextFile()
{
	CStringArray list(TZDB_FILE_LIST);
	auto name = list[fileIndex++];
	if(!name) {
		Serial << F("All files downloaded") << endl;
		return;
	}
	auto request = new HttpRequest(F(TZDB_URL) + name);
	request->onBody(onRequestBody);
	request->onRequestComplete(onDownload);
	downloadClient.send(request);
}

void gotIP(IpAddress ip, IpAddress, IpAddress)
{
	Serial << _F("Connected. Got IP: ") << ip << endl;

	requestNextFile();
}

void connectFail(const String&, MacAddress, WifiDisconnectReason)
{
	Serial.println(F("I'm NOT CONNECTED!"));
}

#ifdef ARCH_HOST
[[maybe_unused]] void parseFile(const String& name)
{
	String filename = F("/stripe/sandboxes/tzdata/tzdb-2024a/") + name;
	File file(&IFS::Host::getFileSystem());
	if(!file.open(filename)) {
		Serial << F("Open '") << name << "': " << file.getLastErrorString() << endl;
		return;
	}

	Serial << F("Parsing '") << name << "'" << endl;

	char buffer[990];
	int len;
	while((len = file.read(buffer, sizeof(buffer))) > 0) {
		size_t offset{0};
		while(parser.push(buffer, len, offset)) {
			handleRow(parser.getRow());
		}
	}
	endParse();
	Serial << "OK, parse done" << endl;
}

[[maybe_unused]] void parseDatabase()
{
	CStringArray list(TZDB_FILE_LIST);
	for(auto name : list) {
		parseFile(name);
	}
	Serial << F("All files parsed") << endl;
}
#endif

void printHeap()
{
	Serial << _F("Heap statistics") << endl;
	Serial << _F("  Free bytes:  ") << system_get_free_heap_size() << endl;
#ifdef ENABLE_MALLOC_COUNT
	Serial << _F("  Used:        ") << MallocCount::getCurrent() << endl;
	Serial << _F("  Peak used:   ") << MallocCount::getPeak() << endl;
	Serial << _F("  Allocations: ") << MallocCount::getAllocCount() << endl;
	Serial << _F("  Total used:  ") << MallocCount::getTotal() << endl;
#endif
}

} // namespace

// #define USE_HOST_FILESYSTEM
// #define HOST_FILE_TEST

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true); // Allow debug print to serial

#ifdef USE_HOST_FILESYSTEM
	auto fs = new IFS::Host::FileSystem("out/host");
	fs->mount();
	fileSetFileSystem(fs);
#else
	lfs_mount();
#endif

	int err = createDirectories(F("updates/rules/"));
	Serial << F("Create directories: ") << fileGetErrorString(err) << endl;

	IFS::Debug::listDirectory(Serial, *IFS::getDefaultFileSystem(), nullptr, IFS::Debug::Option::recurse);

	IFS::FileSystem::Info info{};
	fileGetSystemInfo(info);
	Serial << info;

	auto timer = new SimpleTimer;
	timer->initializeMs<5000>(printHeap);
	timer->start();

#ifdef HOST_FILE_TEST
	parseDatabase();
#else

	// Setup the WIFI connection
	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD); // Put your SSID and password here

	WifiEvents.onStationGotIP(gotIP);
	WifiEvents.onStationDisconnect(connectFail);
#endif
}
