#pragma once

#include<Bela.h>
#include <libraries/Midi/Midi.h>
#include <string>

class ResourceManager;
class BelaInterface;
class DeviceMap;
class Voices;
class Samplers;
class Loopers;
#include "IDisplayContext.h"
#include "IMidi.h"

//#include "../interfaces/Controller.h"


//TODO: put this in the controller somewhere.
/*
struct ZipperFreeGain {
    float g_current = 0.0f; // linear gain used on samples
    float g_target  = 0.0f; // linear target from pot
    int   rampSamples = 0;
    int   rampLeft    = 0;
    float g_step      = 0.0f;

    void setRampMs(float ms, float fs) {
        rampSamples = std::max(1, int(ms * 1e-3f * fs));
    }

    // set target in dB; map pot p∈[0,1] → dB range
    void setTargetFromPot(float p, float dBmin=-60.0f, float dBmax=0.0f) {
        float dB = dBmin + p * (dBmax - dBmin);
        g_target = std::pow(10.0f, dB / 20.0f);
        rampLeft = rampSamples;
        g_step = (g_target - g_current) / std::max(1, rampLeft);
    }

    inline float nextSampleGain() {
        if (rampLeft > 0) {
            g_current += g_step;
            --rampLeft;
        } else {
            g_current = g_target;
        }
        return g_current;
    }
};
*/


enum class UIState {
	General,
    Instrument,
    Sampler,
    COUNT //when cast to int gives the number of states
};

class Controller {
public:

	Controller(ResourceManager* resourceManager);
	
	void processBlockwise();

	float process(float inFrame);
	
	void stateSwitch(int indexShift);
	
	void setDisplay();
	
	void updateLooperLights();
	
	bool toggleRecord(int looper);
	
	bool togglePlay(int looper);
	
	bool inSamplerMode() {return state == UIState::Sampler;}

	float getVolumeMet() {return metronomeGain;}
	
	float getVolumeTotal() {return outputGain;}
	
	float getInputGain() {return inputGain;}

	
private:
	ResourceManager* resourceManager;
	BelaInterface* interface;
	DeviceMap* deviceMap;
	Voices* voices;
	Samplers* samplers;
	Loopers* loopers;
	IDisplayContext* displayContext;
	IMidi* keyMidi;
	IMidi* controlMidi;
	SamplePack* keyInstrumentSamplePack;
	SamplePack* drumInstrumentSamplePack;

	UIState state = UIState::General;
	int bpm = 120;
	float outputGain = 1.0f;
	float metronomeGain = 1.0f;
	float inputGain = 1.0;
	float instrumentGain = 1.0f;
	int currentInstrumentIndex = 0;
	std::string currentInstrumentString = "Piano";
	int currentSamplerIndex = 0;
	int autoSliceNumber = 5;
	int currentSliceIndex = 0;
	
	int numberOfLoopers = 8; //TODO: make this more flexible.
	bool looperStateHasChanged = false;
};