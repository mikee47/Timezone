#pragma once

#include <WHashMap.h>
#include <memory>

class CountryMap
{
public:
	void load(const String& filename);

	const char* operator[](const char* code)
	{
		return code ? map[makeCode(code[0], code[1])] : nullptr;
	}

	// Convert ASCII 2-digit country code into number for efficient indexing
	static constexpr uint16_t makeCode(char c1, char c2)
	{
		return (c1 << 8) | c2;
	}

private:
	HashMap<uint16_t, const char*> map;
	std::unique_ptr<char[]> names;
};
