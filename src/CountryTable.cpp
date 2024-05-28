#include "include/Timezone/CountryTable.h"
#include <Data/Stream/FileStream.h>

#define MAX_LINE_LENGTH 64

CountryTable::CountryTable(const String& filename) : Table(new FileStream(filename), '\t', "", MAX_LINE_LENGTH)
{
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
