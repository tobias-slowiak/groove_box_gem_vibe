#pragma once
//compile
#include <vector>
#include <math.h>
#include <cassert>

class ResourceManager;
class BasicUtilities;
#include "ADSR.h"
#include "StreamingBuffer.h"

//TODO: iteratorptr is unelegant and also i think i only delete voices via stealing, would be better if they
//got removed when done
class Voice {
public:
	Voice(StreamingBufferIterator& iterator, int note, float playbackRate,
		ResourceManager* resourceManager,
		float attack = 0.01f, float decay = 0.0f, float sustain = 1.0f, float release = 0.0f,
	    bool repeat = false);
	
    void noteOff();
        
	float process();
	
	int getNote(){return note;}
	
	int getSamplePos(){return (int)position;}
	
	bool isOn(){return adsr.isOn();}
	
private:
	friend class Voices;
	StreamingBufferIterator* iteratorPtr;
	int note;
	int velocity;
	float playbackRate = 1.0f;
	bool repeat = false;

	ResourceManager* resourceManager;
	size_t requestId;

	float leftFrame = 0.0f;
	float rightFrame = 0.0f;
	
	bool playbackRateIsOne = false;

	double position = 0.0;
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
		int note, float playbackRate, bool repeat = false,
		float attack = 0.0f, float decay = 0.0f, float sustain = 1.0f, float release = 0.1f);
	
	//TODO: trigger off all velocities of this note.
	void triggerOff(int note);

    const int maxVoices = 32;  // Max number of simultaneous voices

private:
	ResourceManager* resourceManager;
	std::vector<Voice> activeVoices;
};
