/*
249 items, total text length (with NUL terminator) is 2628 bytes.
Total RAM size is (249 * 2) + 2628 = 3126 bytes.

Checking using `system_get_free_heap_size`.

(1) Old heap allocator
(2) New heap allocator
								(1)		(2)
	std::map<Code, String>		14528	11720
	HashMap<Code, String> 		18960	9800
	std::map<Code, uint16_t>	12608	8608
	HashMap<Code, const char*> 			6624
	HashMap<Code, uint16_t> 			6128
	CountryTable				3144	3136
 */

#include "include/Timezone/CountryMap.h"

CountryMap::CountryMap(CountryTable& table)
{
	size_t size{0};
	for(auto country : table) {
		size += 2 + strlen(country.name()) + 1;
	}

	list.reserve(size);
	String line;
	for(auto country : table) {
		auto code = country.code();
		line = code.a;
		line += code.b;
		line += country.name();
		list += line;
	}
}

const char* CountryMap::operator[](Country::Code code)
{
	for(auto line : list) {
		if(code == Country::Code(line)) {
			return line + 2;
		}
	}
	return nullptr;
}
