#include "../../include/hardwareInterfaces/RotaryEncoder.h"
#include "../../include/hardwareInterfaces/Button.h"
//compiel

void RotaryEncoder::processBlockwise(){
	if(!initialized || context == nullptr){
		event = RotaryEncoderEvent::None;
		return;
	}

	pushButton.processBlockwise();
	if(pushButton.pressed()){
		event = RotaryEncoderEvent::Push;
		return; //TODO: if i get a push and a left/right in the same block something gets lost.
	}

	const uint8_t a = (digitalRead(context, 0, pinA) == LOW) ? 1 : 0;
	const uint8_t b = (digitalRead(context, 0, pinB) == LOW) ? 1 : 0;
	const uint8_t currentState = static_cast<uint8_t>((a << 1) | b);

	// On startup or after re-init, seed previous state to avoid a bogus step.
	if(!hasPrevState){
		prevState = currentState;
		hasPrevState = true;
		event = RotaryEncoderEvent::None;
		return;
	}

	// Valid Gray-code transitions only. Invalid jumps (bounce/noise) are ignored.
	static const int8_t kTransitionDelta[16] = {
		0,  1, -1,  0,
		-1, 0,  0,  1,
		1,  0,  0, -1,
		0, -1,  1,  0
	};

	const uint8_t transition = static_cast<uint8_t>((prevState << 2) | currentState);
	prevState = currentState;
	const int delta = kTransitionDelta[transition];

	if(delta != 0){
		quarterSteps += delta;
		if(quarterSteps >= 4){
			quarterSteps = 0;
			event = RotaryEncoderEvent::Left;
			return;
		}
		if(quarterSteps <= -4){
			quarterSteps = 0;
			event = RotaryEncoderEvent::Right;
			return;
		}
	}

	event = RotaryEncoderEvent::None;
}
