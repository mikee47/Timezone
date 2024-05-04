#pragma once

#include <WHashMap.h>
#include <memory>

class CountryMap : public HashMap<uint16_t, const char*>
{
public:
	void load(const String& filename);

	using HashMap::operator[];

	const char* operator[](const char* code)
	{
		return code ? operator[](makeCode(code[0], code[1])) : nullptr;
	}

	// Convert ASCII 2-digit country code into number for efficient indexing
	static constexpr uint16_t makeCode(char c1, char c2)
	{
		return (c1 << 8) | c2;
	}

	static constexpr uint16_t makeCode(const char* s)
	{
		return s ? makeCode(s[0], s[1]) : 0;
	}

	static String getCodeString(uint16_t code)
	{
		char buf[]{char(code >> 8), char(code)};
		return String(buf, 2);
	}

private:
	std::unique_ptr<char[]> names;
};
