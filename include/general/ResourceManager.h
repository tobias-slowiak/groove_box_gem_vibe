#pragma once

#include <Bela.h>

#include <memory>
#include <math.h>
#include <vector>
#include <stdexcept>
#include <string>
#include <atomic>

// Forward declarations for classes used in method signatures
class BelaInterface;
class IDisplayContext;
class IMidi;

// Include complete types needed for std::unique_ptr members
#include "ModeManager.h"
#include "../hardwareInterfaces/DeviceMap.h"
#include "../hardwareInterfaces/IMidi.h"
#include "Metronome.h"
#include "../audio/Samplers.h"
#include "../audio/Loopers.h"
#include "../audio/SamplePack.h"
#include "../audio/Mixer.h"
#include "../audio/InstrumentCatalog.h"
#include "../ui/UI.h"
#include "../ui/InputHandler.h"
#include "../hardwareInterfaces/LooperLights.h"
#include "../audio/SignalRouter.h"
#include "../audio/Recorder.h"


class ResourceManager{
public:
	ResourceManager(){};
	
	void setup(BelaContext* context);
	
	void cleanup(BelaContext* context);

	void processBlockwise();
	
	//#########PUTTERS AND GETTERS
	BelaContext* getBelaContext();
	
	ModeManager& getModeManager();
	
	DeviceMap& getDeviceMap();
	
	IDisplayContext& getDisplayContext();

	Metronome& getMetronome();
	
	Samplers& getSamplers();
	
	Loopers& getLoopers();

	SignalRouter& getSignalRouter();

	Recorder& getRecorder();
	
	UI& getUI();

	InstrumentCatalog& getInstrumentCatalog();

	DrumCatalog& getDrumCatalog();

	Mixer& getMixer();
	
	BelaInterface* getBelaInterface();
	
	IMidi& getKeyMidi();
	
	IMidi& getControlMidi();

	SamplePack& getKeyInstrumentSamplePack();
	
	SamplePack& getDrumSamplePack();

	LooperLights& getLooperLights();
	
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
	float UpperLimitInputGain = 20.0f;
	bool inMonoMode = true; //Maybe someday implement stereo
	bool interfaceConnected = false;
	bool keyMidiConnected = false;
	bool controlMidiConnected = false;
	bool keysInMelodicMode = true; //keys vs drums
	float END_OF_SAMPLE = -999999.0f;
	std::string SAMPLES_PATH = "/mnt/sdcard/Samples/";
	int NUM_LINES_PER_DISPLAY = 4;
	
private:
	BelaContext*context = nullptr;
	std::unique_ptr<ModeManager> modeManager;
	std::unique_ptr<DeviceMap> deviceMap;
	BelaInterface* interface = nullptr;
	
	std::vector<float> frames;
	std::unique_ptr<Metronome> metronome;
	std::unique_ptr<Samplers> samplers;
	std::unique_ptr<Loopers> loopers;
	std::unique_ptr<IDisplayContext> displayContext;
	std::unique_ptr<IMidi> keyMidi;
	std::unique_ptr<IMidi> controlMidi;
	std::unique_ptr<SamplePack> keyInstrumentSamplePack;
	std::unique_ptr<SamplePack> drumSamplePack;
	std::unique_ptr<Mixer> mixer;
	std::unique_ptr<SignalRouter> signalRouter;
	std::unique_ptr<UI> ui;
	std::unique_ptr<InputHandler> inputHandler;
	std::unique_ptr<LooperLights> looperLights;
	std::unique_ptr<Recorder> recorder;
	InstrumentCatalog instrumentCatalog;
	DrumCatalog drumCatalog;
	
	std::vector<float>* testSample = nullptr;
	int testSampleSize = 0;
	std::atomic<bool> updateDisplayFlag;
};
