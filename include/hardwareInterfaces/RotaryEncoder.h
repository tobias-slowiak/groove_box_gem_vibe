#pragma once
//compiel
#include <vector>
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
	RotaryEncoder(BelaContext* context, int index, int pinA, int pinB, int pinS): index(index){
		buttons = {Button(context, pinA), Button(context, pinB), Button(context, pinS)};
	}
	
    void processBlockwise();
    
    int getIndex() {return index;}
    
    RotaryEncoderEvent getEvent() {return event;}
    
private:
	int index = 0;
	std::vector<bool> pending = {false, false}; // left right
	std::vector<Button> buttons;
	RotaryEncoderEvent event = RotaryEncoderEvent::None;
};