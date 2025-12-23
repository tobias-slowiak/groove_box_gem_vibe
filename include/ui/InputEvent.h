#pragma once

enum class InputSource {
	Interface,
	Midi
};

enum class InputEventType {
	Button,
	ButtonHold,
	Pot,
	Encoder,
	MidiNoteOn,
	MidiNoteOff,
	MidiControlChange,
	MidiOther,
	Unknown
};

struct InputEvent {
	InputSource source = InputSource::Interface;
	InputEventType type = InputEventType::Unknown;
	int id = -1;
	float value = 0.0f;
	int data0 = 0;
	int data1 = 0;
};
