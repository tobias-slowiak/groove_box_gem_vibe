#pragma once

#include <Bela.h>

class Button {
public:
	Button(){}
	Button(BelaContext* context, int pin): context(context), pin(pin) {}
	
    void processBlockwise();
	
	bool pressed() {return buttonPressed;}
	
	bool pressedHold() {return buttonPressedHold;}
	
	int getPin() { return pin;}
    
private:
	BelaContext* context = nullptr;
	int pin = 0;
	int inputPinState = HIGH;
	int oldInputPinState = HIGH;
	bool buttonPressed = false;
	int buttonPressedFor = 0;
	bool buttonPressedHold = false;
	unsigned int inactiveCounter = 0;
};