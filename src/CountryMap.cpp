#include "include/Timezone/CountryMap.h"
#include <Data/CsvReader.h>
#include <Data/Stream/FileStream.h>

void CountryMap::load(const String& filename)
{
	clear();
	names.reset();

	CsvReader csv(new FileStream(filename), '\t', nullptr, 256);

	// Make initial pass to determine required buffer sizes
	size_t count{0};
	size_t nameSize{0};
	while(csv.next()) {
		++count;
		auto name = csv.getValue(1);
		nameSize += strlen(name) + 1;
	}

	// We can pre-allocate storage to reduce heap fragmentation
	if(!allocate(count)) {
		return;
	}
	names.reset(new char[nameSize]);
	if(!names) {
		return;
	}

	// Second pass to store data
	csv.reset();
	auto nameptr = names.get();
	while(csv.next()) {
		auto s = csv.getValue(int(0));
		auto code = makeCode(s[0], s[1]);
		HashMap::operator[](code) = nameptr;

		auto name = csv.getValue(1);
		auto len = strlen(name);
		memcpy(nameptr, name, len + 1);
		nameptr += len + 1;
	}
}
