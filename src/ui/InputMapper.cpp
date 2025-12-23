#include "../../include/ui/InputMapper.h"

std::vector<UIAction> InputMapper::map(const std::vector<InputEvent>& events) const {
	std::vector<UIAction> actions;
	actions.reserve(events.size());
	for(const auto& event : events) {
		UIAction action;
		action.id = event.id;
		action.value = event.value;
		action.data0 = event.data0;
		action.data1 = event.data1;
		switch(event.type) {
			case InputEventType::Button:
				action.type = UIActionType::Button;
				break;
			case InputEventType::ButtonHold:
				action.type = UIActionType::ButtonHold;
				break;
			case InputEventType::Pot:
				action.type = UIActionType::Pot;
				break;
			case InputEventType::Encoder:
				action.type = UIActionType::Encoder;
				break;
			case InputEventType::MidiNoteOn:
				action.type = UIActionType::MidiNoteOn;
				break;
			case InputEventType::MidiNoteOff:
				action.type = UIActionType::MidiNoteOff;
				break;
			case InputEventType::MidiControlChange:
				action.type = UIActionType::MidiControlChange;
				break;
			case InputEventType::MidiOther:
				action.type = UIActionType::MidiOther;
				break;
			case InputEventType::Unknown:
			default:
				action.type = UIActionType::None;
				break;
		}
		actions.push_back(action);
	}
	return actions;
}
