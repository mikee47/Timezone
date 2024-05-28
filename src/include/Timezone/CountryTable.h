#pragma once

#include <CSV/Table.h>

struct Country : public CSV::Record {
	/**
	  * @brief 2-character country code
	  */
	union Code {
		struct {
			char a;
			char b;
		};
		uint16_t value;

		constexpr Code() : value(0)
		{
		}

		constexpr Code(char a, char b) : a(a), b(b)
		{
		}

		constexpr Code(const char* s) : a(s ? s[0] : 0), b(s ? s[1] : 0)
		{
		}

		explicit operator String() const
		{
			return String(&a, 2);
		}

		constexpr bool operator==(Code other) const
		{
			return value == other.value;
		}
	};

	enum ColumnIndex {
		col_code,
		col_name,
	};

	using Record::Record;

	Code code() const
	{
		return Code(row[col_code]);
	}

	operator Code() const
	{
		return code();
	}

	const char* name() const
	{
		return row[col_name];
	}

	operator const char*() const
	{
		return name();
	}

	bool operator==(Code code) const
	{
		return this->code() == code;
	}
};

class CountryTable : public CSV::Table<Country>
{
public:
	CountryTable(const String& filename);

	Country operator[](Country::Code code);
};
