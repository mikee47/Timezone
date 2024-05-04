#include "include/Timezone/ZoneTable.h"
#include <Data/Stream/FileStream.h>

void ZoneTable::load(const String& filename)
{
	csv = std::make_unique<CsvReader>(new FileStream(filename), '\t', nullptr, 256);
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
