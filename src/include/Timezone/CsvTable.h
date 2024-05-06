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
template <class Record> class CsvTable
{
public:
	template <typename... Params> void load(Params... params)
	{
		csv = std::make_unique<CsvReader>(params...);
	}

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
	Record next()
	{
		if(csv && csv->next()) {
			return Record(csv->getRow());
		}
		return Record();
	}

protected:
	std::unique_ptr<CsvReader> csv;
};
