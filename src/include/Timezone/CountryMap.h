#pragma once

#include "CountryTable.h"
#include <Data/CStringArray.h>

/**
 * @brief If code -> name lookups are required this class provides a memory-efficient map
 *
 * Table content is stored in a single block of allocated RAM using the most compact
 * form possible: "IEIreland\0" "IMIsle of Man\0", etc.
 */
class CountryMap
{
public:
	CountryMap(CountryTable& table);

	const char* operator[](Country::Code code);

private:
	CStringArray list;
};
