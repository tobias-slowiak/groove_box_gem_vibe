#include <string>
#include <map>
//compiel

#include "../../include/hardwareInterfaces/DeviceMap.h"
#include "../../include/general/ResourceManager.h"

#define LAUNCHKEY_46_MK1

DeviceMap::DeviceMap(ResourceManager& resourceManager){

	#ifdef BELA_INTERFACE_V0
		inputGainPin = 5;
		metVolumePin = 6;
		totalVolumePin = 7;
		reversePottis = false;
		
	#endif
	
	
	#ifdef BELA_INTERFACE_V1
		pottiPinToGainId = {
		    {0, GainId::Master},
		    {1, GainId::Instrument},
		    {2, GainId::Metronome},
		    {3, GainId::InputL},
		    {4, GainId::InputR}
		};
		numberOfButtons = 16;
		numberOfRotEncs = 2;
		rotEncPins = {{12,15,14},{6,10,7}};
		buttonPins = {0,1,2,3,4,5,8,9,11,13};
		buttonDown = 9;
		buttonUp = 8;
		buttonLeft = 11;
		buttonRight = 13;
		buttonMasterPlay = 0; 
		buttonMasterRecord = 1;
		buttonKeyDrumToggle = 2;
		buttonMicToggle = 3;
		buttonMetronomeState = 4;
		buttonFIVE = 5;
		numberOfPotentiometers = 8;
		reversePottis = true;
		outputGainPotentiometerIndex = 0;
		metronomeGainPotentiometerIndex = 1;
		inputGainPotentiometerIndex = 2;
		instrumentGainPotentiometerNumber = 3;
		
		
	#endif
	
	
	#ifdef LAUNCHKEY_46_MK1
		keyMidiName = "hw:1,0,0";//TODO: does this reliably work or should i get the name of the midi device and then let the program find the address via the name if thats possible.
		controlMidiName = "hw:1,0,1";
		
		LED_GREEN = 17;
		LED_RED = 120;
		LED_OFF = 0;
		LED_STATUS_BYTE = 0x90;
		LED_SOLID_ON_CHANNEL = 15;
		LED_FLASHING_ON_CHANNEL = 1;
		
		
		volumeControlToLooper = {
		    {21, 0},
		    {22, 1},
		    {23, 2},
		    {24, 3},
		    {25, 4},
		    {26, 5},
		    {27, 6},
		    {28, 7}
		};
		
		playControlToLooperId = {
		    {36, 0},
		    {97, 1},
		    {98, 2},
		    {99, 3},
		    {100, 4},
		    {101, 5},
		    {102, 6},
		    {103, 7}
		};
		looperToPlayControl = {
		    {0,96},
		    {1,97},
		    {2,98},
		    {3,99},
		    {4,100},
		    {5,101},
		    {6,102},
		    {7,103}
		};
		
		recControlToLooperId = {
		    {37, 0},
		    {113, 1},
		    {114, 2},
		    {115, 3},
		    {116, 4},
		    {117, 5},
		    {118, 6},
		    {119, 7},
		};
		
		looperToRecControl = {
		    {0,112},
		    {1,113},
		    {2,114},
		    {3,115},
		    {4,116},
		    {5,117},
		    {6,118},
		    {7,119},
		};
	#endif

	#ifdef LAUNCHKEY_37_MK3
		keyMidiName = "hw:1,0,0";//TODO: does this reliably work or should i get the name of the midi device and then let the program find the address via the name if thats possible.
		controlMidiName = "hw:1,0,1";

		LED_GREEN = 17;
		LED_RED = 120;
		LED_OFF = 0;
		LED_STATUS_BYTE = 0x90;
		LED_SOLID_ON_CHANNEL = 15;
		LED_FLASHING_ON_CHANNEL = 1;
		
		volumeControlToLooper = {
		    {21, 0},
		    {22, 1},
		    {23, 2},
		    {24, 3},
		    {25, 4},
		    {26, 5},
		    {27, 6},
		    {28, 7}
		};
		
		playControlToLooperId = {
		    {36, 0},
		    {97, 1},
		    {98, 2},
		    {99, 3},
		    {100, 4},
		    {101, 5},
		    {102, 6},
		    {103, 7}
		};
		looperToPlayControl = {
		    {0,96},
		    {1,97},
		    {2,98},
		    {3,99},
		    {4,100},
		    {5,101},
		    {6,102},
		    {7,103}
		};
		
		recControlToLooperId = {
		    {37, 0},
		    {113, 1},
		    {114, 2},
		    {115, 3},
		    {116, 4},
		    {117, 5},
		    {118, 6},
		    {119, 7},
		};
		
		looperToRecControl = {
		    {0,112},
		    {1,113},
		    {2,114},
		    {3,115},
		    {4,116},
		    {5,117},
		    {6,118},
		    {7,119},
		};
	#endif
}
