#include <Bela.h>
#include <vector>

#include "../include/Samplers.h"

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
    if(bufferReadIndex < numberOfFrames)
        samplerBuffer.at(bufferReadIndex++) = in;
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
	return {samplerBuffer.data() + starts.at(sliceIndex), ends.at(sliceIndex) - starts.at(sliceIndex)};
}

void Sampler::setStart(int sliceIndex, float percentage) {
	int newStart = percentage * numberOfFrames;
	if(sliceIndex < numberOfSlices)
		if(newStart < ends.at(sliceIndex))
			starts.at(sliceIndex) = newStart;
}

void Sampler::setEnd(int sliceIndex, float percentage) {
	int newEnd = percentage * numberOfFrames;
	if(sliceIndex < numberOfSlices)
		if(newEnd > starts.at(sliceIndex))
			ends.at(sliceIndex) = newEnd;
}

float Sampler::getFrame(int index) {
	if(index < numberOfFrames)
		return samplerBuffer.at(index);
	return 0.0f;
}

float Sampler::getPlaybackRate(int note) {
	int sliceIndex = noteToSliceIndex(note);
	if(sliceIndex >= 0 && sliceIndex < playbackRates.size())
		return playbackRates.at(sliceIndex);
	else
		return 1.0f;
}




void Samplers::newSampler(float timeInSeconds) {
	samplers.push_back(Sampler(timeInSeconds));
}

void Samplers::startRecord(int samplerIndex) {
	samplers.at(samplerIndex).startRecord();
}

bool Samplers::isRecording(int samplerIndex) {
	return samplers.at(samplerIndex).isRecording();
}

void Samplers::process(float inFrame) {
	for(auto&sampler: samplers){
		if(sampler.isRecording()) sampler.recordFrame(inFrame);
	}
}

void Samplers::newSlice(int samplerIndex) {
	samplers.at(samplerIndex).newSlice();
}

int Samplers::getNrSlices(int samplerIndex) {
	return samplers.at(samplerIndex).getNrSlices();
}

std::pair<float*, int> Samplers::getSampleSlice(int samplerIndex, int sliceIndex) {
	return samplers.at(samplerIndex).getSampleSlice(sliceIndex);
}

void Samplers::setStart(int samplerIndex, int sliceIndex, float percentage) {
	samplers.at(samplerIndex).setStart(sliceIndex, percentage);
}
void Samplers::setEnd(int samplerIndex, int sliceIndex, float percentage) {
	samplers.at(samplerIndex).setEnd(sliceIndex, percentage);
}
float Samplers::getFrame(int samplerIndex, int index) {
	return samplers.at(samplerIndex).getFrame(index);
}

float Samplers::getPlaybackRate(int samplerIndex, int note) {
	return samplers.at(samplerIndex).getFrame(note);
}

