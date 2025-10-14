#include "../include/Voices.h"

#include "../include/ADSR.h"
#include "../include/ResourceManager.h"
#include "../include/BasicUtilities.h"

Voice::Voice(std::pair<float*,int> sample, int note, ResourceManager* resourceManager, float attack, float decay, float sustain, float release, float gain, float playbackRate, bool repeat)
:  note(note), resourceManager(resourceManager), gain(gain), playbackRate(playbackRate), repeat(repeat) {
	adsr = ADSR{attack, decay, sustain, release, resourceManager};
	adsr.init();
	this->sample = sample.first;
	this->sampleSize = sample.second;
}

    
float Voice::process(){ // TODO: unelegant with the tuple, do differently
	if((int)position + 1 >= sampleSize){
		if(repeat) position = 0.0f;
		else{
			adsr.instantOff();
			return 0.0f;
		}
	}
	float frame = 0.0f;
	if(floatIsEqual(playbackRate, 1.0f)){
		frame = sample[(int)position];
	}
	else{
		int leftIdx = (int)position;
		float diff = position - (float)leftIdx;
		if(leftIdx < sampleSize - 2){
			frame = sample[leftIdx] * (1-diff) + sample[leftIdx + 1] * diff;
		}
	}
	position += playbackRate;
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


void Voices::triggerVoice(std::pair<float*, int> sample, int note, float attack, float decay, float sustain, float release, float gain, float playbackRate, bool repeat){
	Voice newVoice = Voice(sample, note, resourceManager, attack, decay, sustain, release, gain, playbackRate, repeat);
	if((int)activeVoices.size() >= maxVoices) {
        // Voice stealing: remove the oldest voice
        activeVoices.erase(activeVoices.begin());
    }
	activeVoices.push_back(newVoice);
}

void Voices::triggerOff(int note){
	for(int i = activeVoices.size() - 1; i >= 0; i--){
		if(activeVoices.at(i).getNote() == note){
			activeVoices.at(i).noteOff();
		}
	}
}

