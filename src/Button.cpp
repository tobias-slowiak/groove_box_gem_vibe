#include "../include/Button.h"
#include <cassert>

int BLOCKS_BEFORE_NEXT_PRESS = 250/16;
int BLOCKS_TO_PRESSED_HOLD = 2700;
void Button::processBlockwise(){
	assert(context != nullptr);
	buttonPressed = false;
	inactiveCounter++;
	oldInputPinState = inputPinState;
	inputPinState = digitalRead(context, 0, pin);
	
	if(inputPinState == LOW){
		buttonPressedFor++;
	}
	if(inputPinState == HIGH){
		buttonPressedFor = 0;
	}
	if(buttonPressedHold){
		if(inputPinState == HIGH){
		buttonPressedHold = false;
		buttonPressedFor = 0;
		}
	}
	
	if(buttonPressedFor > BLOCKS_TO_PRESSED_HOLD) buttonPressedHold = true;
	
	if(inactiveCounter < BLOCKS_BEFORE_NEXT_PRESS) return;//pressed only possible after 
	if(inputPinState == LOW && oldInputPinState != LOW){
		buttonPressed = true;
		inactiveCounter = 0;
	}
}
