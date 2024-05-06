#pragma once

#include <Data/CsvReader.h>
#include <memory>

class Country
{
public:
	// Convert ASCII 2-digit country code into number for efficient indexing
	union Code {
		struct {
			char a;
			char b;
		};
		uint16_t value;

		Code()
		{
		}

		Code(char a, char b) : a(a), b(b)
		{
		}

		Code(const char* s) : a(s ? s[0] : 0), b(s ? s[1] : 0)
		{
		}

		explicit operator String()
		{
			return String(&a, 2);
		}

		bool operator==(const Code& other) const
		{
			return value == other.value;
		}
	};

	Country()
	{
	}

	Country(const CStringArray& row) : row(&row)
	{
	}

	Code code() const
	{
		return Code(getCell(0));
	}

	operator Code() const
	{
		return code();
	}

	const char* name() const
	{
		return getCell(1);
	}

	operator const char*() const
	{
		return name();
	}

	bool operator==(Code code) const
	{
		return this->code() == code;
	}

	explicit operator bool() const
	{
		return row;
	}

private:
	const char* getCell(unsigned i) const
	{
		return row ? (*row)[i] : nullptr;
	}

	const CStringArray* row{};
};

class CountryMap
{
public:
	void load(const String& filename);

	/**
	 * @brief Reset to start of table
	 */
	void reset()
	{
		if(csv) {
			csv->reset();
		}
	}

	/**
	 * @brief Fetch next record
	 */
	Country next()
	{
		if(csv && csv->next()) {
			return Country(csv->getRow());
		}
		return Country();
	}

	Country operator[](Country::Code code);

private:
	std::unique_ptr<CsvReader> csv;
};
