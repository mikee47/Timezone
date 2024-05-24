#include "Tabulator.h"

void Tabulator::print(const String& text)
{
	unsigned startColumn{column};
	if(column % colWidth) {
		auto count = (column + colWidth - 1) / colWidth;
		startColumn = count * colWidth;
	}

	auto textlen = text.length();
	if(startColumn + textlen > lineWidth) {
		column = 0;
		output.println();
	} else {
		auto padlen = startColumn - column;
		output.print(String().pad(padlen));
		textlen += padlen;
	}
	output.print(text);
	column += textlen;
}
