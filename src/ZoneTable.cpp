#include "include/Timezone/ZoneTable.h"
#include <Data/Stream/FileStream.h>

#define MAX_LINE_LENGTH 150

ZoneTable::ZoneTable(const String& filename) : CsvTable(new FileStream(filename), '\t', nullptr, MAX_LINE_LENGTH)
{
}

String Zone::getContinentCaption(const String& name)
{
	constexpr auto dig2 = [](char c1, char c2) -> uint16_t { return (c1 << 8) | c2; };
	auto strptr = name.c_str();
	switch(dig2(strptr[0], strptr[1])) {
	case dig2('A', 'm'): // America
		return name + 's';
	case dig2('A', 'r'): // Arctic
	case dig2('A', 't'): // Atlantic
	case dig2('I', 'n'): // Indian
	case dig2('P', 'a'): // Pacific
		return name + F(" Ocean");
	default:
		return name;
	}
}

size_t ZoneFilter::match(const String& filter, bool includePathSep)
{
	matches.clear();
	root = "";
	auto filterLen = filter.length();
	int rootLen = filter.lastIndexOf('/') + 1;
	for(auto zone : table) {
		auto name = zone.name();
		auto nameLen = strlen(name);
		if(nameLen < filterLen) {
			continue;
		}
		if(!filter.equalsIgnoreCase(name, filterLen)) {
			continue;
		}
		auto sep = strchr(name + filterLen, '/');
		auto len = nameLen;
		if(sep) {
			len = includePathSep + sep - name;
		}

		if(matches.isEmpty()) {
			root.setString(name, rootLen);
		}

		String s(name + rootLen, len - rootLen);
		if(!matches.contains(s)) {
			matches.add(s);
		}
	}

	if(sorted) {
		sortMatches();
	}

	return matches.count();
}

void ZoneFilter::sortMatches()
{
	matches.sort([](const String& lhs, const String& rhs) -> int { return lhs.compareTo(rhs); });
}
