#include "Tabulator.h"
#include <HardwareSerial.h>
#include <Data/Buffer/LineBuffer.h>

class Menu
{
public:
	using Callback = Delegate<void()>;
	using LineCallback = Delegate<void(String& line)>;

	Menu(Print& output);

	void handleInput(Stream& input);

	void begin(const String& caption);
	void additem(const String& caption, Callback callback);
	void end();
	void custom(const String& prompt, LineCallback submit, LineCallback autoComplete);
	void select(uint8_t choice);
	void prompt();
	void submit(String& line);

private:
	Print& output;
	Tabulator tabulator;
	Vector<Callback> callbacks;
	LineCallback autoCompleteCallback;
	LineCallback submitCallback;
	String commandPrompt;
	LineBuffer<32> buffer;
};
