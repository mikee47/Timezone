#include "include/Timezone/CountryTable.h"
#include <Data/Stream/FileStream.h>

#define MAX_LINE_LENGTH 64

void CountryTable::load(const String& filename)
{
	return CsvTable::load(new FileStream(filename), '\t', nullptr, MAX_LINE_LENGTH);
}

Country CountryTable::operator[](Country::Code code)
{
	reset();
	while(auto country = next()) {
		if(country == code) {
			return country;
		}
	}
	return Country();
}
