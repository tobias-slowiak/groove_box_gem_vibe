#pragma once

enum class UIActionType {
	None,
	StateNext,
	StatePrev,
	Button,
	ButtonHold,
	Pot,
	Encoder,
	MidiNoteOn,
	MidiNoteOff,
	MidiControlChange,
	MidiOther
};

struct UIAction {
	UIActionType type = UIActionType::None;
	int id = -1;
	float value = 0.0f;
	int data0 = 0;
	int data1 = 0;
};
