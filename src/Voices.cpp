#include "../include/Voices.h"
//compile
#include "../include/ADSR.h"
#include "../include/ResourceManager.h"
#include "../include/BasicUtilities.h"
#include "../include/StreamingBuffer.h"
#include <cassert>

Voice::Voice(StreamingBufferIterator& iterator,
	ResourceManager* resourceManager,
	float attack, float decay, float sustain, float release,
	float gain, float playbackRate, bool repeat)
:  iterator(&iterator),
	note(iterator.sampleIdentifier.first), velocity(iterator.sampleIdentifier.second),
	resourceManager(resourceManager),
	gain(gain),
	playbackRate(playbackRate), repeat(repeat) {
	assert(resourceManager != nullptr);
	adsr = ADSR{attack, decay, sustain, release, resourceManager};
	adsr.init();
	if(floatIsEqual(playbackRate, 1.0f)) playbackRateIsOne = true;
}
    
float Voice::process(){ // TODO: unelegant with the tuple, do differently
	assert(iterator != nullptr);
	assert(resourceManager != nullptr);
	if(playbackRateIsOne){
		(*iterator)++;
		return gain * adsr.process() * *(*iterator);
	}
	if((int)(position + playbackRate) > (int)position){
		currentSample = nextSample;
		(*iterator)++;
		nextSample = *(*iterator);
		if(nextSample == resourceManager->END_OF_SAMPLE){
			if(repeat){
				position = 0.0f;
				//TODO: make this loop around.
			}else{
				adsr.instantOff();
				return 0.0f;
			}
		}
	}
	position += playbackRate;
	float frame = 0.0f;
	float diff = position - (int)position;
	frame = currentSample * (1-diff) + nextSample * diff;
	return gain * adsr.process() * frame;
}

	


float Voices::process(){
	float frame = 0.0f;
	for(auto it = activeVoices.begin(); it != activeVoices.end(); ) {
		if(it->isOn()){
        	frame += it->process();
        	++it;
		} else {
			activeVoices.erase(it);
		}
    }
    return frame;
}


void Voices::triggerVoice(StreamingBufferIterator& iterator,
		float gain, float playbackRate, bool repeat,
		float attack, float decay, float sustain, float release){
	Voice newVoice = Voice(iterator,
		resourceManager,
		attack, decay, sustain, release,
		gain, playbackRate, repeat);
	if((int)activeVoices.size() >= maxVoices) {
        // Voice stealing: remove the oldest voice
        activeVoices.erase(activeVoices.begin());
    }
	activeVoices.push_back(newVoice);
}



void Voices::triggerOff(int note){
	for(int i = activeVoices.size() - 1; i >= 0; i--){
		assert(i >= 0 && activeVoices.size() > static_cast<size_t>(i));
		auto& voice = activeVoices.at(i);
		if(voice.getNote() == note){
			voice.noteOff();
		}
	}
}
