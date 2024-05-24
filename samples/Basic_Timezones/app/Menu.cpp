#include "Menu.h"

Menu::Menu(Print& output) : output(output), tabulator(output)
{
}

void Menu::begin(const String& caption)
{
	output << endl << caption << ":" << endl;
	callbacks.clear();
	autoCompleteCallback = nullptr;
	submitCallback = nullptr;
	commandPrompt = "> ";
}

void Menu::additem(const String& caption, Callback callback)
{
	auto choice = callbacks.count() + 1;

	String s;
	s += "  ";
	s += choice;
	s += ") ";
	s += caption;

	tabulator.print(s);

	callbacks.add(callback);
}

void Menu::custom(const String& prompt, LineCallback submit, LineCallback autoComplete)
{
	commandPrompt = prompt;
	submitCallback = submit;
	autoCompleteCallback = autoComplete;
}

void Menu::select(uint8_t choice)
{
	unsigned index = choice - 1;
	if(index >= callbacks.count()) {
		output << "Invalid choice '" << choice << "'" << endl;
		return;
	}
	callbacks[index]();
}

void Menu::prompt()
{
	output << commandPrompt;
}

void Menu::submit(String& line)
{
	if(submitCallback) {
		submitCallback(line);
	} else if(line) {
		auto choice = line.toInt();
		Menu::select(choice);
	}
	prompt();
}

void Menu::end()
{
	tabulator.println();
	if(callbacks.count() == 1) {
		output << '1' << endl;
		select(1);
	} else {
		output << _F("Please select an option (1 - ") << callbacks.count() << ")" << endl;
	}
}

void Menu::handleInput(Stream& input)
{
	using Action = LineBufferBase::Action;

	int c;
	while((c = input.read()) >= 0) {
		if(c == '\t' && Menu::autoCompleteCallback) {
			String line(buffer);
			Menu::autoCompleteCallback(line);
			auto len = buffer.getLength();
			if(line.equals(buffer.getBuffer(), len)) {
				continue;
			}
			while(len--) {
				buffer.processKey('\b', &Serial);
			}
			for(auto c : line) {
				buffer.processKey(c, &Serial);
			}
			continue;
		}
		switch(buffer.processKey(c, &Serial)) {
		case Action::submit: {
			String line(buffer);
			buffer.clear();
			submit(line);
			break;
		}

		case Action::clear:
			prompt();
			break;

		case Action::echo:
		case Action::backspace:
		case Action::none:
			break;
		}
	}
}
