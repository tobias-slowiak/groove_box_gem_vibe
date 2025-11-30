#pragma once

#include <vector>
#include <math.h>
#include <cassert>

class ResourceManager;
class StreamingBuffer;
class StreamingBufferIterator;
class BasicUtilities;
#include "ADSR.h"

class Voice {
public:
	Voice(StreamingBufferIterator& iterator,
		ResourceManager* resourceManager,
		float attack = 0.0f, float decay = 0.0f, float sustain = 1.0f, float release = 0.0f,
		float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false);

    Voice& operator=(const Voice&) = default;
	
    void noteOff(){adsr.noteOff();}
        
	float process();
	
	int getNote(){return note;}
	
	int getSamplePos(){return (int)position;}
	
	bool isOn(){return adsr.isOn();}
	
private:
	StreamingBufferIterator* iterator;
	int note;
	int velocity;
	float gain = 1.0f;
	float playbackRate = 1.0f;
	bool repeat = false;

	ResourceManager* resourceManager;
	size_t requestId;

	float currentSample = 0.0f;
	float nextSample = 0.0f;
	
	bool playbackRateIsOne = false;

	float position = 0.0f;
	ADSR adsr;
};

class Voices {
public:
	Voices(ResourceManager* resourceManager): resourceManager(resourceManager){
		assert(resourceManager != nullptr);
	}
	
    float process();
    
    void clear(){activeVoices.clear();}
    
    void triggerVoice(StreamingBufferIterator& iterator,
		float gain = 1.0f, float playbackRate = 1.0f, bool repeat = false,
		float attack = 0.0f, float decay = 0.0f, float sustain = 1.0f, float release = 0.1f);
	
	//TODO: trigger off all velocities of this note.
	void triggerOff(int note);

    const int maxVoices = 32;  // Max number of simultaneous voices

private:
	ResourceManager* resourceManager;
	std::vector<Voice> activeVoices;
};
