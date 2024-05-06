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

	CsvRecord(const CStringArray& row) : row(row)
	{
	}

	explicit operator bool() const
	{
		return row;
	}

protected:
	const char* getCell(unsigned i) const
	{
		return row[i];
	}

	CStringArray row;
};

/**
 * @brief Class template for accessing CSV file as set of records
 * @tparam Record Class inherited from CsvRecord
 */
template <class Record> class CsvTable : public CsvReader
{
public:
	class Iterator
	{
	public:
		static constexpr unsigned end{UINT32_MAX};

		Iterator(CsvReader* reader, unsigned index) : reader(reader), index(index)
		{
		}

		Iterator& operator++()
		{
			if(reader && reader->next()) {
				++index;
			} else {
				index = end;
			}
			return *this;
		}

		Record operator*() const
		{
			return (reader && index != end) ? Record(reader->getRow()) : Record();
		}

		bool operator==(const Iterator& other) const
		{
			return reader == other.reader && index == other.index;
		}

		bool operator!=(const Iterator& other) const
		{
			return !operator==(other);
		}

	private:
		CsvReader* reader;
		unsigned index;
	};

	using CsvReader::CsvReader;

	/**
	 * @brief Fetch next record
	 */
	Record next()
	{
		return CsvReader::next() ? getRow() : Record();
	}

	Iterator begin()
	{
		reset();
		return CsvTable::next() ? Iterator(this, 0) : end();
	}

	Iterator end()
	{
		return Iterator(this, Iterator::end);
	}
};
