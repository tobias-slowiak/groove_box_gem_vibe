#pragma once

#include <vector>
#include <math.h>

class ResourceManager;
#include "ADSR.h"
class BasicUtilities;

const int maxVoices = 32;  // Max number of simultaneous voices

class Voice {
public:
	Voice(std::pair<float*,int> sample, int note, ResourceManager* resourceManager, float attack = 0.0f, float decay = 0.0f,
			float sustain = 1.0f, float release = 0.0f, float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false);
	Voice(std::vector<float>* sample, int note, ResourceManager* resourceManager, float attack = 0.0f, float decay = 0.0f,
			float sustain = 1.0f, float release = 0.0f, float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false);

    void noteOff(){adsr.noteOff();}
        
	float process();
	
	int getNote(){return note;}

	int getSampleSize(){return sampleSize;}
	
	int getSamplePos(){return (int)position;}
	
	bool isOn(){return adsr.isOn();}
	
private:
	float* sample;
	int sampleSize = 0;
	int note; //this is to remember which key this note corresponds to to noteOff it when the key is released
	
	ResourceManager* resourceManager;
	
	float gain = 1.0f;
	float playbackRate = 1.0f;
	float position = 0.0f;
	bool repeat = false;
	ADSR adsr;
};

class Voices {
public:
	Voices(ResourceManager* resourceManager): resourceManager(resourceManager){}
	
    float process();
    
    void clear(){activeVoices.clear();}
    
    void triggerVoice(std::pair<float*, int> sample, int note, float attack = 0.0f, float decay = 0.0f,
    		float sustain = 1.0f, float release = 0.1f, float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false);
	void triggerVoice(std::vector<float>* sample, int note, float attack = 0.0f, float decay = 0.0f,
    		float sustain = 1.0f, float release = 0.1f, float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false);

	void triggerOff(int note);

    
private:
	ResourceManager* resourceManager;
	std::vector<Voice> activeVoices;
};

