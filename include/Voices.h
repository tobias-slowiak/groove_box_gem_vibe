#pragma once

#include <vector>
#include <math.h>
#include <cassert>

class ResourceManager;
class StreamingBuffer;
class BasicUtilities;
#include "ADSR.h"


const int maxVoices = 32;  // Max number of simultaneous voices

class Voice {
public:
	Voice(StreamingBuffer* buffer, int note, int velocity, ResourceManager* resourceManager, float attack = 0.0f, float decay = 0.0f,
			float sustain = 1.0f, float release = 0.0f, float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false);

    void noteOff(){adsr.noteOff();}
        
	float process();
	
	int getNote(){return note;}
	
	int getSamplePos(){return (int)position;}
	
	bool isOn(){return adsr.isOn();}
	
private:

	ResourceManager* resourceManager;
	StreamingBuffer* buffer;
	size_t requestId;

	int note;
	int velocity;

	float currentSample = 0.0f;
	float nextSample = 0.0f;
	
	float gain = 1.0f;
	float playbackRate = 1.0f;
	bool playbackRateIsOne = false;
	float position = 0.0f;
	bool repeat = false;
	ADSR adsr;
};

class Voices {
public:
	Voices(ResourceManager* resourceManager): resourceManager(resourceManager){
		assert(resourceManager != nullptr);
	}
	
    float process();
    
    void clear(){activeVoices.clear();}
    
    void triggerVoice(StreamingBuffer* buffer, int note, int velocity, float attack = 0.0f, float decay = 0.0f,
    		float sustain = 1.0f, float release = 0.1f, float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false);

	//TODO: trigger off all velocities of this note.
	void triggerOff(int note);

    
private:
	ResourceManager* resourceManager;
	std::vector<Voice> activeVoices;
};
