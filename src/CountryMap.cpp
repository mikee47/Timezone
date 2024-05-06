#include "include/Timezone/CountryMap.h"
#include <Data/Stream/FileStream.h>

#define MAX_LINE_LENGTH 64

void CountryMap::load(const String& filename)
{
	csv = std::make_unique<CsvReader>(new FileStream(filename), '\t', nullptr, MAX_LINE_LENGTH);
}

Country CountryMap::operator[](Country::Code code)
{
	reset();
	while(auto country = next()) {
		if(country == code) {
			return country;
		}
	}
	return Country();
}
