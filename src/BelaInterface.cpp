#include <vector>
#include <string>
#include <queue>
#include <math.h>
#include <cassert>
//test
#include "../include/BelaInterface.h"
#include "../include/BasicUtilities.h"
#include "../include/DeviceMap.h"

#include "../include/ResourceManager.h"
#include "../include/Button.h"
#include "../include/RotaryEncoder.h"
#include "../include/Potentiometer.h"
#include "../include/DebugLog.h"

InterfaceMessage::InterfaceMessage(): type(InterfaceMessageType::none), id(-1), value(-1.0), event(RotaryEncoderEvent::None) {}

void InterfaceMessage::prettyPrint(){
	if(type == InterfaceMessageType::ButtonPressed)
		rt_printf("ButtonPressed: %d\n", id);
	if(type == InterfaceMessageType::ButtonPressedHold)
		rt_printf("ButtonPressedHold: %d\n", id);
	if(type == InterfaceMessageType::RotEncSignal){
		std::string e = "";
		if(event == RotaryEncoderEvent::Right) e = "right";
		if(event == RotaryEncoderEvent::Left) e = "left";
		if(event == RotaryEncoderEvent::Push) e = "push";
		rt_printf("RotEncSignal: %d, %s\n", id, e.c_str());
	}
	/*
	if(type == InterfaceMessageType::PotSignal)
		rt_printf("Potti signal: %d, %f\n", id, value);
		*/
}

void BelaInterface::printPotentiometerValues(){
	rt_printf("Potentiometer Values: ");
	for(auto& potti: potentiometers){
		rt_printf("%f, ", potti.getValue());
	}
	rt_printf("\n");
}

void BelaInterface::newEncoder(BelaContext* context, int index, int pinA, int pinB, int pinS){
	assert(context != nullptr);
	rotaryEncoders.push_back(RotaryEncoder(context, index, pinA, pinB, pinS));
	DEBUG_RT_PRINTF("  Rotary encoder %d pins A:%d B:%d S:%d\n", index, pinA, pinB, pinS);
}

BelaInterface::BelaInterface(ResourceManager* resourceManager): resourceManager(resourceManager) {
	assert(resourceManager != nullptr);
	DeviceMap* dm = resourceManager->getDeviceMap();
	assert(dm != nullptr);
	BelaContext* context = resourceManager->getBelaContext();
	assert(context != nullptr);
	DEBUG_RT_PRINTF("BelaInterface init: buttons=%zu encoders=%d pottis=%d\n",
	                dm->buttonPins.size(),
	                dm->numberOfRotEncs,
	                dm->numberOfPotentiometers);
	for(auto pin: dm->buttonPins){
		buttons.push_back(Button(context, pin));
		DEBUG_RT_PRINTF("  Button pin %d registered\n", pin);
	}
	for(int i = 0; i < dm->numberOfRotEncs; i++){
		assert(dm->rotEncPins.size() > static_cast<size_t>(i));
		auto& encoderPins = dm->rotEncPins.at(i);
		assert(encoderPins.size() > 0);
		int pinA = encoderPins.at(0);
		assert(encoderPins.size() > 1);
		int pinB = encoderPins.at(1);
		assert(encoderPins.size() > 2);
		int pinS = encoderPins.at(2);
		this->newEncoder(context, i, pinA, pinB, pinS);
	}
	for(int i = 0; i < dm->numberOfPotentiometers; i++){
		potentiometers.push_back(Potentiometer(context, dm->reversePottis, i, 20.0f));
	}
}

void BelaInterface::processBlockwise(){
	assert(resourceManager != nullptr);

	for(auto& button: buttons){
		button.processBlockwise();
		if(button.pressed()){
			DEBUG_RT_PRINTF("Button press detected on pin %d\n", button.getPin());
			messages.push(InterfaceMessage(InterfaceMessageType::ButtonPressed, button.getPin()));
		}
		if(button.pressedHold()){
			DEBUG_RT_PRINTF("Button hold detected on pin %d\n", button.getPin());
			messages.push(InterfaceMessage(InterfaceMessageType::ButtonPressedHold, button.getPin()));
		}
	}

	for(auto& rotaryEncoder: rotaryEncoders){
		rotaryEncoder.processBlockwise();
		RotaryEncoderEvent event = rotaryEncoder.getEvent();
		if(event != RotaryEncoderEvent::None){
			DEBUG_RT_PRINTF("RotEnc %d event %d\n", rotaryEncoder.getIndex(), static_cast<int>(event));
			messages.push(InterfaceMessage(InterfaceMessageType::RotEncSignal, rotaryEncoder.getIndex(), 1.0, event));
		}
	}
	
	for(auto& potentiometer: potentiometers){
		potentiometer.processBlockwise();
		if(potentiometer.hasChanged()){
			/*
			DEBUG_RT_PRINTF("Potentiometer %d queued value %.3f\n",
			                potentiometer.getPin(),
			                potentiometer.getValue());
							*/
			messages.push(InterfaceMessage(InterfaceMessageType::PotSignal, potentiometer.getPin(), potentiometer.getValue()));
		}
	}
}


InterfaceMessage BelaInterface::getNextInterfaceMessage() {
	InterfaceMessage msg;
    if (messages.empty()) return msg;
    msg = messages.front();
    messages.pop();
    return msg;
}

int BelaInterface::numAvailableMessages() {
	return messages.size();
}
