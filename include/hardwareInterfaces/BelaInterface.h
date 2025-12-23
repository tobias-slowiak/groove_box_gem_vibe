#pragma once
#include <queue>
#include <vector>

class ResourceManager;
class Button;
#include "RotaryEncoder.h"
#include "Potentiometer.h"

enum class InterfaceMessageType {
	none,
    ButtonPressed,
    ButtonPressedHold,
    PotSignal,
    RotEncSignal
};

class InterfaceMessage {
public:
	InterfaceMessage();
	
	InterfaceMessage(InterfaceMessageType type, int id): type(type), id(id) {}
	
	InterfaceMessage(InterfaceMessageType type, int id, float value): type(type), id(id), value(value) {}
	
	InterfaceMessage(InterfaceMessageType type, int id, float value, RotaryEncoderEvent event): type(type), id(id), value(value), event(event) {}
	
	void prettyPrint();
	
    InterfaceMessageType type;
    int id;
    float value;
    RotaryEncoderEvent event;
};

class BelaInterface {
public:
	BelaInterface(ResourceManager& resourceManager);
	
	void processBlockwise();

    InterfaceMessage getNextInterfaceMessage();
    
    int numAvailableMessages();

    void newEncoder(BelaContext* context, int index, int pinA, int pinB, int pinS);

    void printPotentiometerValues();
    
private:
	ResourceManager& resourceManager;
	std::vector<Button> buttons;
	std::vector<RotaryEncoder> rotaryEncoders;
	std::vector<RotaryEncoderEvent> rotaryEncoderEvents;
	std::vector<Potentiometer> potentiometers;

    std::queue<InterfaceMessage> messages;
};



