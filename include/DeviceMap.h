#pragma once

#include <vector>
#include <string>
#include <map>

class ResourceManager;

#define BELA_INTERFACE_V1
#define LAUNCHKEY_46_MK1
//TODO: make new for launchkey 37 mk3


//BELA Interfaces, necessary varialbes are 

class DeviceMap{
public:
	DeviceMap(ResourceManager* resourceManager);
	
	int inputGainPin;
	int metVolumePin;
	int totalVolumePin;
	int numberOfButtons;
	int numberOfRotEncs;
	std::vector<std::vector<int>> rotEncPins;
	std::vector<int> buttonPins;
	int buttonDown;
	int buttonUp;
	int buttonLeft;
	int buttonRight;
	int buttonZERO; //TODO: THIS doesnt seem to get updated. try again later/see controller.cpp
	int buttonONE;
	int buttonTWO;
	int buttonTHREE;
	int buttonFOUR;
	int buttonFIVE;
	int numberOfPotentiometers;
	bool reversePottis;
	int outputGainPotentiometerIndex;
	int metronomeGainPotentiometerIndex;
	int inputGainPotentiometerIndex;
	int instrumentGainPotentiometerNumber;
		

	//MIDI Device vars
	std::string keyMidiName;
	std::string controlMidiName;
	
	int LED_GREEN;
	int LED_RED;
	int LED_OFF;

	std::map<int,int> volumeControlToLooper;
	
	std::map<int,int> playControlToLooper;
	
	std::map<int,int> looperToPlayControl;
	
	std::map<int,int> recControlToLooper;
	
	std::map<int,int> looperToRecControl;
	
};