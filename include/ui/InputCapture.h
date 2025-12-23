#pragma once

#include <vector>
#include "InputEvent.h"

class BelaInterface;
class IMidi;

class InputCapture {
public:
	InputCapture(BelaInterface& interface, IMidi& keyMidi, IMidi& controlMidi);

	void capture();
	void clear();
	const std::vector<InputEvent>& events() const;

private:
	BelaInterface& interface;
	IMidi& keyMidi;
	IMidi& controlMidi;
	std::vector<InputEvent> captured;
};
