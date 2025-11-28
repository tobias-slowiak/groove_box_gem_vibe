#include <vector>
#include <string>
#include <stdexcept>
#include <math.h>
#include <cassert>
//test...
#include "../include/IMidi.h"
#include "../include/MidiReal.h"
#include "../include/MidiFake.h"
#include "../include/IDisplayContext.h"
#include "../include/DisplayContextReal.h"
#include "../include/DisplayContextFake.h"
#include "../include/Controller.h"
#include "../include/BelaInterface.h"
#include "../include/BasicUtilities.h"
#include "../include/DeviceMap.h"
#include "../include/DebugLog.h"

#include "../include/ResourceManager.h"

Controller::Controller(ResourceManager* resourceManager): resourceManager(resourceManager){
	assert(resourceManager != nullptr);
	interface = resourceManager->getBelaInterface();
	assert(interface != nullptr);
	keyMidi = resourceManager->getKeyMidi();
	assert(keyMidi != nullptr);
	controlMidi = resourceManager->getControlMidi();
	assert(controlMidi != nullptr);
	displayContext = resourceManager->getDisplayContext();
	assert(displayContext != nullptr);
	deviceMap = resourceManager->getDeviceMap();
	assert(deviceMap != nullptr);
	
	currentSamplerIndex = 0;
}


void Controller::processBlockwise(){
	assert(interface != nullptr);
	assert(deviceMap != nullptr);
	assert(keyMidi != nullptr);
	assert(controlMidi != nullptr);
	assert(displayContext != nullptr);
	///////////////////////////////////////////////INPUTS
	///////////////////processing the hardware interface
	
	interface->processBlockwise();
	while(interface->numAvailableMessages() > 0){
		InterfaceMessage iMessage = interface->getNextInterfaceMessage();
		DEBUG_RT_PRINTF("Controller received message type %d id %d value %.3f\n",
		                static_cast<int>(iMessage.type),
		                iMessage.id,
		                iMessage.value);
#ifdef DEBUG_BUILD
		iMessage.prettyPrint();
#endif
		
		if(iMessage.type == InterfaceMessageType::ButtonPressed){
			
			int buttonNumber = iMessage.id;
			if(buttonNumber == deviceMap->buttonUp){
				this->stateSwitch(-1);

			}
			if(buttonNumber == deviceMap->buttonDown){
				this->stateSwitch(+1);
			}
			switch(state){
				case UIState::General:
					DEBUG_RT_PRINTF("Button %d ignored in General state\n", buttonNumber);
					break;
				case UIState::Instrument:
					DEBUG_RT_PRINTF("Button %d handled in Instrument state\n", buttonNumber);
					break;
				case UIState::Sampler:
					if(buttonNumber == deviceMap->buttonLeft) autoSliceNumber--;
					if(buttonNumber == deviceMap->buttonRight) autoSliceNumber++;
					if(buttonNumber == deviceMap->buttonZERO) rt_printf("TODO: autoSlice the current sample\n");
					break;
				case UIState::COUNT:
					break;
			}
			} else if(iMessage.type == InterfaceMessageType::ButtonPressedHold){
				
			} else if(iMessage.type == InterfaceMessageType::PotSignal){
				int potentiometerNumber = iMessage.id;
				float potValue = clamp(iMessage.value, 0.0f, 1.0f);
				DEBUG_RT_PRINTF("Pot message: index %d value %.3f\n", potentiometerNumber, potValue);
				if(potentiometerNumber == deviceMap->outputGainPotentiometerIndex) outputGain = potValue;
				if(potentiometerNumber == deviceMap->metronomeGainPotentiometerIndex) metronomeGain = potValue;
				if(potentiometerNumber == deviceMap->inputGainPotentiometerIndex) inputGain = potValue;
				if(potentiometerNumber == deviceMap->instrumentGainPotentiometerNumber) instrumentGain = potValue;
			} else if(iMessage.type == InterfaceMessageType::RotEncSignal){
				int rotEncNumber = iMessage.id;
				RotaryEncoderEvent event = iMessage.event;
				DEBUG_RT_PRINTF("RotEnc message: index %d event %d state %d\n",
				                rotEncNumber,
				                static_cast<int>(event),
				                static_cast<int>(state));
 			switch(state){
				case UIState::General:
					break;
				case UIState::Instrument:
					break;
				case UIState::Sampler:
					if(rotEncNumber == 0){
						if(event == RotaryEncoderEvent::Left) currentSamplerIndex--; //TODO: make this go around
						if(event == RotaryEncoderEvent::Right) currentSamplerIndex++;
						if(event == RotaryEncoderEvent::Push) rt_printf("TODO: make new Sampler\n");
					} else if(rotEncNumber == 1){
						if(event == RotaryEncoderEvent::Left) currentSliceIndex--; //TODO: make this go around
						if(event == RotaryEncoderEvent::Right) currentSliceIndex++;
						if(event == RotaryEncoderEvent::Push) rt_printf("TODO: make new Slice\n");
					}
					break;
				case UIState::COUNT:
					break;
			}
		}
		this->setDisplay();	
	}
	
	

	/////////////////processing Midi
	IMidiParser* keyParser = keyMidi->getParser();
	assert(keyParser != nullptr);
	while(keyParser->numAvailableMessages() > 0) {
        IMidiChannelMessage* kmMessage = keyParser->getNextChannelMessage();
#ifdef DEBUG_BUILD
        kmMessage->prettyPrint();
#endif
        int note = kmMessage->getDataByte(0);
        int velocity = kmMessage->getDataByte(1);
        if(kmMessage->getChannel() == 0 && kmMessage->getType() != kmmControlChange){
			rt_printf("playing note %d with velocity %d\n", note, velocity);
        }
    }
    
    IMidiParser* controlParser = controlMidi->getParser();
    assert(controlParser != nullptr);
    while(controlParser->numAvailableMessages() > 0) {
    	IMidiChannelMessage* cmMessage = controlParser->getNextChannelMessage();
#ifdef DEBUG_BUILD
    	cmMessage->prettyPrint();
#endif
        int note = cmMessage->getDataByte(0);
        int velocity = cmMessage->getDataByte(1);
    	
    	if(cmMessage->getChannel() == 0){// && cmMessage.getType() == kmmControlChange){
    		rt_printf("control message with note %d and velo %d\n", note, velocity);
    	}
    }
    
    ////////////////////////////////////////////////////////////OUTPUTS
    //////////////////DisplayContext 
	displayContext->processBlockwise();
	
    /////////////////LooperLights
	if(looperStateHasChanged)
		this->updateLooperLights();
}

void Controller::stateSwitch(int indexShift){
	if(indexShift != 1 && indexShift != -1) throw std::runtime_error("stateSwitch() used with indexshift unequal 1 or -1");
	int stateIndex = ((int)state + indexShift) % (int)UIState::COUNT;
	if(stateIndex < 0) stateIndex += (int)UIState::COUNT;
	if(stateIndex == (int)UIState::COUNT) stateIndex += indexShift;
	DEBUG_RT_PRINTF("State switch %d -> %d\n", static_cast<int>(state), stateIndex);
	state = (UIState)(stateIndex);
}

void Controller::setDisplay(){
	assert(displayContext != nullptr);
	//by default UIState::General display
	std::vector<std::vector<std::string>> lines= {{"bpm: " + std::to_string(bpm),
					  "inst: " + currentInstrumentString,
					  "",
					  ""},
					  
					 {"Gain: " + std::to_string(outputGain),
					  "InGain: " + std::to_string(inputGain),
					  "",
					  ""}};
	switch(state){
		case UIState::General:
			break;
		case UIState::Instrument:
			lines = {{"inst: " + currentInstrumentString,
					  ": " ,
					  "",
					  ""},
					  
					 {"InstGain: " + std::to_string(instrumentGain),
					  "",
					  "",
					  ""}};
			break;
		case UIState::Sampler:
			lines = {{"smplr: " + std::to_string(currentSamplerIndex),
					  "NrSamplers: " + std::to_string(2), //TODO: implement getNrSamplers in Samplers class
					  "push for new",
					  ""},
					  
					 {"Slice: " + std::to_string(currentSliceIndex),
					  "NrSlices: " + std::to_string(3), //TODO: make this real
					  "push for new" ,
					  "0: auto: " + std::to_string(autoSliceNumber)}};
			break;
		case UIState::COUNT:
			lines = {{"error: ",
					  "setDisplay()",
					  "should ",
					  "not reach "},
					  
					 {"UIState:: ", //TODO: make this real
					  "COUNT" ,
					  "",
					  ""}};
			break;
	}
	displayContext->setLines(lines);
}


void Controller::updateLooperLights(){
	/*
		looperStateHasChanged = false;
		for(int looperIndex = 0; looperIndex < numberOfLoopers; looperIndex++){
			if(loopers->isPlaying(looperIndex)) controlMidi->writeMessage(0x90, 15, looperToPlayControl.at(looperIndex), LED_COLORS::GREEN);
			else controlMidi->writeMessage(0x90, 15, looperToPlayControl.at(looperIndex), LED_COLORS::OFF);
			LooperState s = loopers->getState(looperIndex);
			if(s == LooperState::Empty) controlMidi->writeMessage(0x90, 15, looperToRecControl.at(looperIndex), LED_COLORS::OFF);
			if(s == LooperState::HoldingOut){
				controlMidi->writeMessage(0x90, 15, looperToRecControl.at(looperIndex), LED_COLORS::RED);
				controlMidi->writeMessage(0x90, 1, looperToRecControl.at(looperIndex), LED_COLORS::OFF); //flashing between red and off
			}
			if(s == LooperState::FirstRecording) controlMidi->writeMessage(0x90, 15, looperToRecControl.at(looperIndex), LED_COLORS::RED);
			if(s == LooperState::RunningOut){
				controlMidi->writeMessage(0x90, 1, looperToRecControl.at(looperIndex), LED_COLORS::OFF); //flashing between red and off
			}
			if(s == LooperState::Recording) controlMidi->writeMessage(0x90, 15, looperToRecControl.at(looperIndex), LED_COLORS::RED);
			if(s == LooperState::NotRecording) controlMidi->writeMessage(0x90, 15, looperToRecControl.at(looperIndex), LED_COLORS::OFF);
		}
		*/
	}
