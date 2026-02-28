#pragma once
#include <vector>
#include <math.h>
#include <cassert>

class ResourceManager;
class BasicUtilities;
#include "ADSR.h"
#include "../streamingBuffer/StreamingBuffer.h"

//TODO: iteratorptr is unelegant and also i think i only delete voices via stealing, would be better if they
//got removed when done
class Voice {
public:
	Voice(StreamingBufferIterator& iterator, int note, float playbackRate,
		ResourceManager& resourceManager,
		float gain = 1.0f,
		float attack = 0.01f, float decay = 0.0f, float sustain = 1.0f, float release = 0.0f,
	    bool repeat = false);

	Voice(Voice&& other) noexcept;
	Voice& operator=(Voice&& other) noexcept;

	Voice(const Voice& other) = delete;
	Voice& operator=(const Voice& other) = delete;
	
    void noteOff();
        
	float process();
	
	int getNote(){return note;}
	
	int getSamplePos(){return (int)position;}
	
	bool isOn(){return adsr.isOn();}
	
private:
	friend class Voices;
	bool rewindForRepeat();
	StreamingBufferIterator* iteratorPtr;
	int note;
	int velocity;
	float playbackRate = 1.0f;
	float gain = 1.0f;
	bool repeat = false;

	ResourceManager& resourceManager;
	size_t requestId;

	float leftFrame = 0.0f;
	float rightFrame = 0.0f;
	bool noteReleased = false;
	
	bool playbackRateIsOne = false;

	double position = 0.0;
	ADSR adsr;
};

class Voices {
public:
	Voices(ResourceManager& resourceManager): resourceManager(resourceManager) {}
		
	    float process();
	    
	    void clear(){activeVoices.clear();}

	    // Ensures at least one slot is available before creating a new iterator/voice.
	    void prepareForNewVoice();
	    
	    void triggerVoice(StreamingBufferIterator& iterator,
			int note, float playbackRate, float gain = 1.0f, bool repeat = false,
			float attack = 0.0f, float decay = 0.0f, float sustain = 1.0f, float release = 0.1f);
	
	//TODO: trigger off all velocities of this note.
	void triggerOff(int note);

    const int maxVoices = 32;  // Max number of simultaneous voices

private:
	bool releaseOldestVoice();
	ResourceManager& resourceManager;
	std::vector<Voice> activeVoices;
};
