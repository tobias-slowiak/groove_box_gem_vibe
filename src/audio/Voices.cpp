#include "../../include/audio/Voices.h"
#include "../../include/audio/ADSR.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/general/BasicUtilities.h"
#include "../../include/streamingBuffer/StreamingBuffer.h"
#include <cassert>
#include <cmath>
#include <utility>
//compiel

//TODO: on every voice created there is 1 block dropped, therefore a click. investigate!
Voice::Voice(StreamingBufferIterator& iterator, int note, float playbackRate,
	ResourceManager& resourceManager,
	float attack, float decay, float sustain, float release,
	bool repeat)
:  iteratorPtr(&iterator),
	note(note),
	velocity(iterator.sampleIdentifier.second),
	playbackRate(playbackRate),
	repeat(repeat),
	resourceManager(resourceManager),
	requestId(0),
	adsr(attack, decay, sustain, release, resourceManager) {
	assert(iteratorPtr != nullptr && "Voice ctor received null iterator");
	assert(iterator.sampleIdentifier.first != ITERATOR_INVALID && "Voice ctor iterator invalid id");
	adsr.init();
	if(floatIsEqual(playbackRate, 1.0f)){
		playbackRateIsOne = true;
	} else {
		playbackRateIsOne = false;
		leftFrame = *iterator;
		iterator++;
		rightFrame = *iterator;
	}
}

Voice::Voice(Voice&& other) noexcept
	: iteratorPtr(other.iteratorPtr),
	  note(other.note),
	  velocity(other.velocity),
	  playbackRate(other.playbackRate),
	  repeat(other.repeat),
	  resourceManager(other.resourceManager),
	  requestId(other.requestId),
	  leftFrame(other.leftFrame),
	  rightFrame(other.rightFrame),
	  playbackRateIsOne(other.playbackRateIsOne),
	  position(other.position),
	  adsr(std::move(other.adsr)) {
	other.iteratorPtr = nullptr;
}

Voice& Voice::operator=(Voice&& other) noexcept {
	if(this != &other) {
		assert(&resourceManager == &other.resourceManager);
		iteratorPtr = other.iteratorPtr;
		note = other.note;
		velocity = other.velocity;
		playbackRate = other.playbackRate;
		repeat = other.repeat;
		requestId = other.requestId;
		leftFrame = other.leftFrame;
		rightFrame = other.rightFrame;
		playbackRateIsOne = other.playbackRateIsOne;
		position = other.position;
		adsr = std::move(other.adsr);
		other.iteratorPtr = nullptr;
	}
	return *this;
}

void Voice::noteOff(){
	adsr.noteOff();
}
    
float Voice::process(){ // TODO: unelegant with the tuple, do differently
	assert(iteratorPtr != nullptr && "Voice::process iteratorPtr null");
	StreamingBufferIterator& iterator = *iteratorPtr;
	if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
		adsr.instantOff();
		iteratorPtr = nullptr;
		return 0.0f;
	}
	assert(iterator.sampleIdentifier.first != ITERATOR_INVALID && "Voice::process iterator invalid id");
	float frame = *iterator;
	if(frame == END_OF_SAMPLE){
		adsr.instantOff();
		iteratorPtr = nullptr;
		return 0.0f;
	}
	if(playbackRateIsOne){
		frame = adsr.process() * frame;
		position += 1.0;
		iterator++;
		return frame;
	}
	// advance whole-frame steps first to keep interpolation stable
	double newPosition = position + static_cast<double>(playbackRate);
	int currentIndex = static_cast<int>(position);
	int targetIndex = static_cast<int>(newPosition);
	int steps = targetIndex - currentIndex;
	for(int i = 0; i < steps; ++i){
		leftFrame = rightFrame;
		iterator++;
		rightFrame = *iterator;
		if(rightFrame == END_OF_SAMPLE){
			adsr.instantOff();
			iteratorPtr = nullptr;
			return adsr.process() * leftFrame;
		}
	}
	position = newPosition;
	double intPart = 0.0;
	double frac = std::modf(position, &intPart);
	if(frac < 0.0) frac = 0.0;
	if(frac > 1.0) frac = 1.0;
	float interpolate = static_cast<float>(leftFrame * (1.0 - frac) + rightFrame * frac);
	return adsr.process() * interpolate;
}

	


float Voices::process(){
	float frame = 0.0f;
	for(auto it = activeVoices.begin(); it != activeVoices.end(); ) {
		if(it->isOn()){
        	frame += it->process();
        	++it;
		} else {
			if(it->iteratorPtr) //iteratorPtr might be nulled by voice::process
				it->iteratorPtr->release();
			it = activeVoices.erase(it);
		}
    }
    return frame;
}


void Voices::triggerVoice(StreamingBufferIterator& iterator,
		int note, float playbackRate, bool repeat,
		float attack, float decay, float sustain, float release){
	if((int)activeVoices.size() >= maxVoices) {
        // Voice stealing: remove the oldest voice
        activeVoices.erase(activeVoices.begin());
    }
	assert(iterator.sampleIdentifier.first != ITERATOR_INVALID && "Voices::triggerVoice iterator invalid id");
	activeVoices.push_back({iterator, note, playbackRate,
		resourceManager,
		attack, decay, sustain, release, repeat});
}



void Voices::triggerOff(int note){
	for(int i = activeVoices.size() - 1; i >= 0; i--){
		assert(i >= 0 && activeVoices.size() > static_cast<size_t>(i));
		auto& voice = activeVoices.at(i);
		if(voice.getNote() == note){
			rt_printf("trying to noteOff of note %d with current voice note: %d\n", note, voice.getNote());
			voice.noteOff();
		}
	}
}
