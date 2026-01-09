#include <Bela.h>
#include <vector>
#include <cassert>
//compiel
#include "../../include/audio/Samplers.h"

int noteToSliceIndex(int note){
	return note - 60; //right now most primitive version, just C4 as base 
}

Sampler::Sampler(float timeInSeconds){
	rt_printf("creating real sampler\n");
	numberOfFrames = 44100 * timeInSeconds;
    samplerBuffer.resize(numberOfFrames , 0.0f);
    this->newSlice();
}

void Sampler::startRecord() {
	recording = true;
	bufferReadIndex = 0;
}

bool Sampler::isRecording() {return recording;}

void Sampler::recordFrame(float in){
    if(bufferReadIndex < numberOfFrames){
        assert(samplerBuffer.size() > static_cast<size_t>(bufferReadIndex));
        samplerBuffer.at(bufferReadIndex++) = in;
    }
    else
        recording = false;
}

void Sampler::newSlice(){
	numberOfSlices++;
	playbackRates.push_back(1.0f);
	starts.push_back(0);
	ends.push_back(numberOfFrames - 1);
}

int Sampler::getNrSlices() {return numberOfSlices;}

std::pair<float*, int> Sampler::getSampleSlice(int sliceIndex) {
	assert(sliceIndex >= 0 && starts.size() > static_cast<size_t>(sliceIndex));
	int sliceStart = starts.at(sliceIndex);
	assert(ends.size() > static_cast<size_t>(sliceIndex));
	int sliceEnd = ends.at(sliceIndex);
	assert(sliceStart >= 0);
	assert(sliceEnd >= sliceStart);
	assert(samplerBuffer.size() > static_cast<size_t>(sliceStart));
	assert(samplerBuffer.size() >= static_cast<size_t>(sliceEnd));
	return {samplerBuffer.data() + sliceStart, sliceEnd - sliceStart};
}

void Sampler::setStart(int sliceIndex, float percentage) {
	int newStart = percentage * numberOfFrames;
	assert(newStart >= 0);
	assert(newStart < numberOfFrames);
	if(sliceIndex < numberOfSlices){
		assert(sliceIndex >= 0 && ends.size() > static_cast<size_t>(sliceIndex));
		if(newStart < ends.at(sliceIndex)){
			assert(starts.size() > static_cast<size_t>(sliceIndex));
			starts.at(sliceIndex) = newStart;
		}
	}
}

void Sampler::setEnd(int sliceIndex, float percentage) {
	int newEnd = percentage * numberOfFrames;
	assert(newEnd >= 0);
	assert(newEnd <= numberOfFrames);
	if(sliceIndex < numberOfSlices){
		assert(sliceIndex >= 0 && starts.size() > static_cast<size_t>(sliceIndex));
		if(newEnd > starts.at(sliceIndex)){
			assert(ends.size() > static_cast<size_t>(sliceIndex));
			ends.at(sliceIndex) = newEnd;
		}
	}
}

float Sampler::getFrame(int index) {
	if(index < numberOfFrames){
		assert(samplerBuffer.size() > static_cast<size_t>(index));
		return samplerBuffer.at(index);
	}
	return 0.0f;
}

float Sampler::getPlaybackRate(int note) {
	int sliceIndex = noteToSliceIndex(note);
	if(sliceIndex >= 0 && sliceIndex < playbackRates.size()){
		assert(playbackRates.size() > static_cast<size_t>(sliceIndex));
		return playbackRates.at(sliceIndex);
	}
	else
		return 1.0f;
}




void Samplers::newSampler(float timeInSeconds) {
	samplers.push_back(Sampler(timeInSeconds));
}

void Samplers::startRecord(int samplerIndex) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	samplers.at(samplerIndex).startRecord();
}

bool Samplers::isRecording(int samplerIndex) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	return samplers.at(samplerIndex).isRecording();
}

void Samplers::process(float inFrame) {
	for(auto&sampler: samplers){
		if(sampler.isRecording()) sampler.recordFrame(inFrame);
	}
}

void Samplers::newSlice(int samplerIndex) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	samplers.at(samplerIndex).newSlice();
}

int Samplers::getNrSlices(int samplerIndex) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	return samplers.at(samplerIndex).getNrSlices();
}

std::pair<float*, int> Samplers::getSampleSlice(int samplerIndex, int sliceIndex) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	return samplers.at(samplerIndex).getSampleSlice(sliceIndex);
}

void Samplers::setStart(int samplerIndex, int sliceIndex, float percentage) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	samplers.at(samplerIndex).setStart(sliceIndex, percentage);
}
void Samplers::setEnd(int samplerIndex, int sliceIndex, float percentage) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	samplers.at(samplerIndex).setEnd(sliceIndex, percentage);
}
float Samplers::getFrame(int samplerIndex, int index) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	return samplers.at(samplerIndex).getFrame(index);
}

float Samplers::getPlaybackRate(int samplerIndex, int note) {
	assert(samplerIndex >= 0 && samplers.size() > static_cast<size_t>(samplerIndex));
	return samplers.at(samplerIndex).getFrame(note);
}
