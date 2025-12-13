#pragma once
//compile
#include <Bela.h>

#include <math.h>
#include <vector>
#include <stdexcept>
#include <string>
#include <atomic>

class ModeManager;
class DeviceMap;
class BelaInterface;
class Controller;
class Voices;
class Samplers;
class Loopers;
class IDisplayContext;
class DisplayContextReal;
class DisplayContextFake;
class IMidi;
class SamplePack;


class ResourceManager{
public:
	ResourceManager(){};
	
	void setup(BelaContext* context);
	
	void cleanup(BelaContext* context);
	
	//#########PUTTERS AND GETTERS
	BelaContext* getBelaContext();
	
	ModeManager* getModeManager();
	
	DeviceMap* getDeviceMap();
	
	IDisplayContext* getDisplayContext();
	
	Samplers* getSamplers();
	
	Voices* getVoices();
	
	Loopers* getLoopers();
	
	Controller* getController();
	
	BelaInterface* getBelaInterface();
	
	IMidi* getKeyMidi();
	
	IMidi* getControlMidi();

	SamplePack* getKeyInstrumentSamplePack();
	
	
	std::vector<float>* makeTestSample();
	
	std::pair<float*, int> getTestSample();
	
	std::vector<float>* getTestSampleVector();
	
	std::atomic<bool>& getUpdateDisplayFlag();
	
	
	///////////////////////////////PUBLIC VARIABLES
	int audioFramesPerAnalogFrame;
	int audioFramesPerSecond;
	int audioInputChannels;
	int audioFramesPerBlock;
	int blocksPerSecond;
	float LowerDBLimit = -60.0f;
	float UpperDBLimit = 20.0f;
	float UpperLimitInputGain = 20.0;
	bool inMonoMode = true; //Maybe someday implement stereo
	bool interfaceConnected = false;
	bool keyMidiConnected = false;
	bool controlMidiConnected = false;
	float END_OF_SAMPLE = -999999.0f;
	std::string SAMPLES_PATH = "/mnt/sdcard/Samples/";
private:
	BelaContext*context = nullptr;
	ModeManager* modeManager = nullptr;
	DeviceMap* deviceMap = nullptr;
	BelaInterface* interface = nullptr;
	
	Controller* controller= nullptr;
	Voices* voices= nullptr;
	Samplers* samplers= nullptr;
	Loopers* loopers = nullptr;
	IDisplayContext* displayContext = nullptr;
	IMidi* keyMidi = nullptr;
	IMidi* controlMidi = nullptr;
	SamplePack* keyInstrumentSamplePack = nullptr;
	
	std::vector<float>* testSample = nullptr;
	int testSampleSize = 0;
	std::atomic<bool> updateDisplayFlag;
};
