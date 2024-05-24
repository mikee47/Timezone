#pragma once

#include <Print.h>

/**
 * @brief Class to print items in aligned columns
 */
class Tabulator
{
public:
	/**
     * @brief Constructor
     * @param output
     * @param colWidth Nominal column width
     * @param lineWidth Do not exceed this line length
     */
	Tabulator(Print& output, unsigned colWidth = 25, unsigned lineWidth = 100)
		: output(output), colWidth(colWidth), lineWidth(lineWidth)
	{
	}

	/**
     * @brief Print a cell. May occupy more than one column.
     */
	void print(const String& text);

	/**
     * @brief Reset to start of first column.
     * Call when finished or to forceably start a new row of output.
     */
	void println()
	{
		output.println();
		column = 0;
	}

private:
	Print& output;
	unsigned colWidth;
	unsigned lineWidth;
	unsigned column{0};
};
