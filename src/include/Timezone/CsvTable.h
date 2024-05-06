#pragma once

#include <Data/CsvReader.h>
#include <memory>

/**
 * @brief Base class for interpreting a record (line) in a CSV file
 */
class CsvRecord
{
public:
	CsvRecord()
	{
	}

	CsvRecord(const CStringArray& row) : row(&row)
	{
	}

	explicit operator bool() const
	{
		return row;
	}

protected:
	const char* getCell(unsigned i) const
	{
		return row ? (*row)[i] : nullptr;
	}

	const CStringArray* row{};
};

/**
 * @brief Class template for accessing CSV file as set of records
 * @tparam Record Class inherited from CsvRecord
 */
template <class Record> class CsvTable : public CsvReader
{
public:
	using CsvReader::CsvReader;

	/**
	 * @brief Fetch next record
	 */
	Record next()
	{
		if(CsvReader::next()) {
			return Record(getRow());
		}
		return Record();
	}
};
