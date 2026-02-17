#pragma once
//compiel
#include <cstdint>
#include "Button.h"

//TODO: there is a problem that i cant seem to fix: wenn ich den knopf drehe kommt bei kleiner drehung ein pressed signal und dann nach dem hüpfen zur nächsten position das andere pressed signal. manchmal drehe ich zu weit und dann kommt
//signal A und dann B aber dann durch das leicht weiterdrehen nochmal A. wenn so zB links gedrück wurde und ich rechts drehe wird links gemessen, weil die AB abfolge um 1 verschoben ist. ein time delay (also zB 5 ms warten bis )
//das nächste signal erlaubt ist, scheint nicht zu funktionieren.

enum class RotaryEncoderEvent {
    None,
    Right,
    Left,
    Push
};

class RotaryEncoder {
public:
	RotaryEncoder(){}
	RotaryEncoder(BelaContext* context, int index, int pinA, int pinB, int pinS) { init(context, index, pinA, pinB, pinS); }
	void init(BelaContext* inContext, int inIndex, int inPinA, int inPinB, int pinS){
		context = inContext;
		index = inIndex;
		pinA = inPinA;
		pinB = inPinB;
		pushButton = Button(inContext, pinS);
		initialized = (inContext != nullptr);
		prevState = 0;
		hasPrevState = false;
		quarterSteps = 0;
		event = RotaryEncoderEvent::None;
	}
	
    void processBlockwise();
    
    int getIndex() {return index;}
    
    RotaryEncoderEvent getEvent() {return event;}
    
private:
	BelaContext* context = nullptr;
	int pinA = 0;
	int pinB = 0;
	int index = 0;
	Button pushButton;
	bool initialized = false;
	uint8_t prevState = 0; // packed AB state: (A << 1) | B
	bool hasPrevState = false;
	int quarterSteps = 0;  // accumulate valid quadrature transitions
	RotaryEncoderEvent event = RotaryEncoderEvent::None;
};
