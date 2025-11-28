#include "../include/Voices.h"
//compile
#include "../include/ADSR.h"
#include "../include/ResourceManager.h"
#include "../include/BasicUtilities.h"
#include "../include/StreamingBuffer.h"
#include <cassert>

Voice::Voice(StreamingBuffer* buffer, int note, int velocity, ResourceManager* resourceManager, float attack, float decay, float sustain, float release, float gain, float playbackRate, bool repeat)
:  note(note), velocity(velocity), resourceManager(resourceManager), buffer(buffer),
	gain(gain),
	playbackRate(playbackRate), repeat(repeat) {
	assert(resourceManager != nullptr);
	assert(buffer != nullptr);
	adsr = ADSR{attack, decay, sustain, release, resourceManager};
	adsr.init();
	if(floatIsEqual(playbackRate, 1.0f)) playbackRateIsOne = true;
	requestId = buffer->requestSample({note, velocity});
}
    
float Voice::process(){ // TODO: unelegant with the tuple, do differently
	assert(buffer != nullptr);
	assert(resourceManager != nullptr);
	if((int)(position + playbackRate) > (int)position){
		currentSample = nextSample;
		nextSample = buffer->getNextSample(requestId);
		if(nextSample == resourceManager->END_OF_SAMPLE){
			if(repeat){
				position = 0.0f;
				requestId = buffer->requestSample({note, velocity});
			}else{
				adsr.instantOff();
				return 0.0f;
			}
		}
	}
	position += playbackRate;
	float frame = 0.0f;
	if(playbackRateIsOne){
		frame = currentSample;
	}
	else{
		float diff = position - (int)position;
		frame = currentSample * (1-diff) + nextSample * diff;
	}
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


void Voices::triggerVoice(StreamingBuffer* buffer, int note, int velocity, float attack, float decay, float sustain, float release, float gain, float playbackRate, bool repeat){
	assert(buffer != nullptr);
	Voice newVoice = Voice(buffer, note, velocity, resourceManager, attack, decay, sustain, release, gain, playbackRate, repeat);
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
