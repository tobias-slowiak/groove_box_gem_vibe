#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "../audio/Mixer.h"
#include "../hardwareInterfaces/IMidi.h"
//compiel
class ResourceManager;

//TODO: make new for launchkey 37 mk3

#define LAUNCHKEY_37_MK3
#define BELA_INTERFACE_V1_GEM

//BELA Interfaces, necessary varialbes are 

class DeviceMap{
public:
	DeviceMap(ResourceManager& resourceManager);
	
	std::unordered_map<int, GainId> pottiPinToGainId;
	int midiSetDrumModeChannel = 15;
	int midiControlChange = 0xB0;

	int midiRecordByte1 = 117;
	int midiSamplerRecordByte1 = 76;

	int midiSetDrumModeByte1 = 0x03;
	int midiSetDrumModeByte2 = 0x01;
	int midiInstrumentChannel = 0;
	int midiDrumChannel = -1; // TODO: this is not used yet
	int midiLoooperChannel = 9;
	int midiLooperUndoByte1 = 77;
	int midiLooperEraseByte1 = 116;
	int numberOfButtons;
	int numberOfRotEncs;
	std::vector<std::vector<int>> rotEncPins;
	std::vector<int> buttonPins;
	int buttonDown;
	int buttonUp;
	int buttonLeft;
	int buttonRight;
	int buttonMasterPlay;
	int buttonMasterRecord;
	int buttonKeyDrumToggle;
	int buttonMicToggle;
	int buttonMetronomeState;
	int buttonFIVE;
	int buttonSamplerRecord = 6;
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
	midi_byte_t LED_STATUS_BYTE = 0x90;
	midi_byte_t LED_SOLID_ON_CHANNEL = 15;
	midi_byte_t LED_FLASHING_ON_CHANNEL = 1;

	int initialLooperNumber = 8;

	std::unordered_map<int,int> volumeControlToLooper;
	
	std::unordered_map<int,int> playControlToLooperId;
	
	std::unordered_map<int,int> looperToPlayControl;
	
	std::unordered_map<int,int> recControlToLooperId;
	
	std::unordered_map<int,int> looperToRecControl;
	
};
