#include "../../include/ui/InputHandler.h"
#include "../../include/hardwareInterfaces/BelaInterface.h"
#include "../../include/ui/UI.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/hardwareInterfaces/DeviceMap.h"

InputHandler::InputHandler(ResourceManager& rm)
: rm(rm), ui(rm.getUI()), deviceMap(rm.getDeviceMap())
{}

void InputHandler::parseMessages(){
	//parse Interface Messages
    BelaInterface& interface = *(rm.getBelaInterface());
	while(interface.numAvailableMessages() > 0) {
		InterfaceMessage message = interface.getNextInterfaceMessage();
		handleMessage(message);
	}
	//Parse Midi Messages
	IMidiParser& kParser = *(rm.getKeyMidi().getParser());
	IMidiParser& cParser = *(rm.getControlMidi().getParser());
	while(kParser.numAvailableMessages() > 0) {
		IMidiChannelMessage* message = kParser.getNextChannelMessage();
		if(!message) throw std::runtime_error("problem with keymidi message pointer, is null");
		message->prettyPrint();
		handleMessage(*message);
	}
	while(cParser.numAvailableMessages() > 0) {
		IMidiChannelMessage* message = cParser.getNextChannelMessage();
		if(!message) throw std::runtime_error("problem with controlmidi message pointer, is null");
		message->prettyPrint();
		handleMessage(*message);
	}
}

void InputHandler::handleMessage(InterfaceMessage& message){
	RotaryEncoderEvent event = message.event;

	switch(message.type){
		case InterfaceMessageType::ButtonPressed:
			if(message.id == deviceMap.buttonUp){
				ui.stateSwitch(-1);
				return;
			}
			if(message.id == deviceMap.buttonDown){
				ui.stateSwitch(+1);
				return;
			}
            if(message.id == deviceMap.buttonMasterPlay){
                ui.masterTogglePlay();
                return;
            }
            if(message.id == deviceMap.buttonMasterRecord){
                ui.masterToggleRecord();
                return;
            }
            if(message.id == deviceMap.buttonKeyDrumToggle){
                ui.drumKeySwitch();
                return;
            }
            if(message.id == deviceMap.buttonMicToggle){
                ui.micToggle();
                return;
            }
            if(message.id == deviceMap.buttonMetronomeState){
                ui.metronomeStateSwitch();
                return;
            } 
			break;
		case InterfaceMessageType::RotEncSignal:
			if(event == RotaryEncoderEvent::Right)
                ui.menuUp(message.id);
            if(event == RotaryEncoderEvent::Left)
                ui.menuDown(message.id);
            if(event == RotaryEncoderEvent::Push)
                ui.menuPush(message.id);
			break;
		case InterfaceMessageType::PotSignal:
			if(message.id < (int)GainId::COUNT){
                GainId gainId = deviceMap.pottiPinToGainId[message.id];
				ui.setGain(gainId, message.value);
            }
			break;
		default:
            //TODO, ignore other message types for now
			break;
	}
}

void InputHandler::handleMessage(IMidiChannelMessage& message){

	switch(message.getType()) {
		case kmmControlChange:
			//TODO: looper thing
			break;
		case kmmNoteOn:
			if(message.getChannel() == deviceMap.midiInstrumentChannel){
				ui.triggerVoice(
					message.getDataByte(0),
					message.getDataByte(1),
					rm.keysInMelodicMode
				);
			}
			if(message.getChannel() == deviceMap.midiDrumChannel){
				ui.triggerVoice(
					message.getDataByte(0),
					message.getDataByte(1),
					false
				);
			}
			if(message.getChannel() == deviceMap.midiLoooperChannel){
                //TODO make this more efficient
                if(deviceMap.playControlToLooperId.find(message.getDataByte(0)) != deviceMap.playControlToLooperId.end()){
                    int looperId = deviceMap.playControlToLooperId[message.getDataByte(0)];
                    ui.looperTogglePlay(looperId);
                }
                if(deviceMap.recControlToLooperId.find(message.getDataByte(0)) != deviceMap.recControlToLooperId.end()){
                    int looperId = deviceMap.recControlToLooperId[message.getDataByte(0)];
                    ui.looperToggleRecord(looperId);
                }
                //TODO: check if the following really is OK this way
                //TODO: logarithm to map volume controls to looper ids
                if(message.getDataByte(0) >= 96 && message.getDataByte(0) <= 103){
                    //this is a volume control for looper
                    int looperId = message.getDataByte(0) - 96;
                    float value = message.getDataByte(1) / 127.0f;
                    ui.setLooperGain(looperId, value);
                }
			}
			break;
		case kmmNoteOff:
			if(message.getChannel() == deviceMap.midiInstrumentChannel){
				ui.triggerOff(
					message.getDataByte(0),
					rm.keysInMelodicMode
				);
			}
            if(message.getChannel() == deviceMap.midiDrumChannel){
                ui.triggerOff(
                    message.getDataByte(0),
                    false
                );
            }
			break;
		default:
			//TODO, ignore other message types for now
			break;
	}
}