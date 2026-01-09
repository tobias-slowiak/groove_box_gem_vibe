#include "../../include/hardwareInterfaces/RotaryEncoder.h"
#include "../../include/hardwareInterfaces/Button.h"
#include <cassert>
//compiel

void RotaryEncoder::processBlockwise(){
	for(auto& button: buttons) button.processBlockwise();
	
	assert(buttons.size() > 2);
	if(buttons.at(2).pressed()){
		event = RotaryEncoderEvent::Push;
		return; //TODO: if i get a push and a left/right in the same block something gets lost.
	}
	assert(pending.size() > 0);
	const bool isPendingZeroFalse = pending.at(0) == false;
	assert(pending.size() > 1);
	const bool isPendingOneFalse = pending.at(1) == false;
	if(isPendingZeroFalse && isPendingOneFalse){
		assert(buttons.size() > 0);
		if(buttons.at(0).pressed()){
			assert(pending.size() > 0);
			pending.at(0) = true;
		} else {
			assert(buttons.size() > 1);
			if(buttons.at(1).pressed()){
				assert(pending.size() > 1);
				pending.at(1) = true;
			}
		}
	}
	assert(buttons.size() > 0);
	assert(pending.size() > 1);
	if(buttons.at(0).pressed() && pending.at(1)){
		for(int i = 0; i < 2; i++){
			assert(static_cast<size_t>(i) < pending.size());
			pending.at(i) = false;
		}
		event = RotaryEncoderEvent::Left;
		return;
	}
	assert(buttons.size() > 1);
	assert(pending.size() > 0);
	if(buttons.at(1).pressed() && pending.at(0)){
		for(int i = 0; i < 2; i++){
			assert(static_cast<size_t>(i) < pending.size());
			pending.at(i) = false;
		}
		event = RotaryEncoderEvent::Right;
		return;
	}
	event = RotaryEncoderEvent::None;
}
