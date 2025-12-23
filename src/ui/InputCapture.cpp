#include "../../include/ui/InputCapture.h"
#include "../../include/hardwareInterfaces/BelaInterface.h"
#include "../../include/hardwareInterfaces/IMidi.h"

namespace {
InputEvent toInputEvent(const InterfaceMessage& message) {
	InputEvent event;
	event.source = InputSource::Interface;
	event.id = message.id;
	event.value = message.value;
	event.data0 = static_cast<int>(message.event);
	switch(message.type) {
		case InterfaceMessageType::ButtonPressed:
			event.type = InputEventType::Button;
			break;
		case InterfaceMessageType::ButtonPressedHold:
			event.type = InputEventType::ButtonHold;
			break;
		case InterfaceMessageType::PotSignal:
			event.type = InputEventType::Pot;
			break;
		case InterfaceMessageType::RotEncSignal:
			event.type = InputEventType::Encoder;
			break;
		case InterfaceMessageType::none:
		default:
			event.type = InputEventType::Unknown;
			break;
	}
	return event;
}

void drainMidi(IMidi& midi, std::vector<InputEvent>& out) {
	IMidiParser* parser = midi.getParser();
	if(!parser) {
		return;
	}
	while(parser->numAvailableMessages() > 0) {
		IMidiChannelMessage* message = parser->getNextChannelMessage();
		if(!message) {
			break;
		}
		InputEvent event;
		event.source = InputSource::Midi;
		event.id = message->getChannel();
		event.data0 = message->getDataByte(0);
		event.data1 = message->getDataByte(1);
		event.value = static_cast<float>(event.data1) / 127.0f;
		switch(message->getType()) {
			case kmmNoteOn:
				event.type = InputEventType::MidiNoteOn;
				break;
			case kmmNoteOff:
				event.type = InputEventType::MidiNoteOff;
				break;
			case kmmControlChange:
				event.type = InputEventType::MidiControlChange;
				break;
			default:
				event.type = InputEventType::MidiOther;
				break;
		}
		out.push_back(event);
	}
}
}

InputCapture::InputCapture(BelaInterface& interface, IMidi& keyMidi, IMidi& controlMidi)
	: interface(interface),
	  keyMidi(keyMidi),
	  controlMidi(controlMidi) {}

void InputCapture::capture() {
	captured.clear();
	interface.processBlockwise();
	while(interface.numAvailableMessages() > 0) {
		InterfaceMessage message = interface.getNextInterfaceMessage();
		captured.push_back(toInputEvent(message));
	}
	drainMidi(keyMidi, captured);
	drainMidi(controlMidi, captured);
}

void InputCapture::clear() {
	captured.clear();
}

const std::vector<InputEvent>& InputCapture::events() const {
	return captured;
}
