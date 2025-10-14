#include "../include/RotaryEncoder.h"
#include "../include/Button.h"


void RotaryEncoder::processBlockwise(){
	for(auto& button: buttons) button.processBlockwise();
	
	if(buttons.at(2).pressed()){
		event = RotaryEncoderEvent::Push;
		return; //TODO: if i get a push and a left/right in the same block something gets lost.
	}
	if(pending.at(0) == false && pending.at(1) == false){
		if(buttons.at(0).pressed()){
			pending.at(0) = true;
		} else if(buttons.at(1).pressed()){
			pending.at(1) = true;
		}
	}
	if(buttons.at(0).pressed() && pending.at(1)){
		for(int i = 0; i < 2; i++) pending.at(i) = false;
		event = RotaryEncoderEvent::Left;
		return;
	}
	if(buttons.at(1).pressed() && pending.at(0)){
		for(int i = 0; i < 2; i++) pending.at(i) = false;
		event = RotaryEncoderEvent::Right;
		return;
	}
	event = RotaryEncoderEvent::None;
}