#include <vector>
#include <stdexcept>
#include <cassert>
//compiel
#include "../../include/audio/Loopers.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/general/Metronome.h"


	
float Looper::getProgress(){
	if(loopLengthInFrames == 0) return 0.0f;
	return (float)position / (float)loopLengthInFrames;
}


bool Looper::toggleRecord() {
	if(waitingForBarStart){
		rt_printf("still waiting for bar start, cannot toggle record\n");
		return recording;
	}
	if(loopLengthInFrames == 0){
		if(parentPtr->looperTriggerMode == LooperTriggerMode::Free){
			if(recording){
				//stop recording and set loop length instantly
				setNewLoopLength = true;
				recording = !recording;
				if(parentPtr->autoplay) playing = true;
			} else {
				StreamingBufferIterator& iterator = parentPtr->streamingBuffer.begin({0, looperIndex}, SBIType::Write);
				iteratorPtr = &iterator;
				//start instant recording
				recording = !recording;
			}
		} else if (parentPtr->looperTriggerMode == LooperTriggerMode::OnBar){
			if(recording){
				rt_printf("waiting for bar start to stop recording and set loop length\n");
			} else {
				rt_printf("waiting for bar start to start recording\n");
				StreamingBufferIterator& iterator = parentPtr->streamingBuffer.begin({0, looperIndex}, SBIType::Write);
				iteratorPtr = &iterator;
			}
			waitingForBarStart = true;
		}
	} else {
		recording = !recording;
	}
	//TODO: control when to flush.
	return recording;
}

bool Looper::togglePlay() {
	playing = !playing;
	return playing;
}


float Looper::process(float inFrame){
	static int beat = 0;
	if(waitingForBarStart){
		if(parentPtr->metronome.getBeatsElapsed() == 0){
			rt_printf("bar started,");
			waitingForBarStart = false;
			if(recording){
				rt_printf("setting loop length to %d frames\n", position);
				setNewLoopLength = true;
				recording = false;
			} else {
				rt_printf("starting recording\n");
				recording = true;
			}
			if(recording && parentPtr->autoplay){
				playing = true;
			}
		} else {
			int newbeat = parentPtr->metronome.getBeatsElapsed();
			if(newbeat != beat){
				beat = newbeat;
				rt_printf("current beat %d\n", beat);
			}
		}
		if(!recording){
			return 0.0f;
		}
	}
	if(setNewLoopLength){
		loopLengthInFrames = position + 1;
		printf("set loop length to %d frames with position %d\n", loopLengthInFrames, position);
		parentPtr->availableLoopers[{0, looperIndex}] = loopLengthInFrames;
		StreamingBufferIterator& iterator = *iteratorPtr;
		float prevFrame = *iterator;
		if(recording) *iterator = prevFrame + inFrame;
		iterator++;
		position++;
		*iterator = END_OF_SAMPLE;
		setNewLoopLength = false;
		if(recording) iteratorPtr->flush();
		if(playing){
			return prevFrame;
		}
		return 0.0f;
	}
	if(iteratorPtr){
		if(loopLengthInFrames != 0 && position >= loopLengthInFrames){
			position = 0;
			//TODO: maybe make iterator settable to beginning. this avoids some searches within streamingbuffer.
			iteratorPtr->rewind();
		}
		StreamingBufferIterator& iterator = *iteratorPtr;
		float prevFrame = *iterator;
		if(recording) *iterator = prevFrame + inFrame;
		iterator++;
		position++;
		if(playing){
			return prevFrame;
		}
		return 0.0f;
	}
	return 0.0f;
}




Loopers::Loopers(ResourceManager& resourceManager):
	resourceManager(resourceManager),
	metronome(resourceManager.getMetronome()),
	numberOfLoopers(2),//TODO: get this from device map or other
	availableLoopers({{{0,0}, TOTAL_BUFFER_FRAMES}, {{0,1}, TOTAL_BUFFER_FRAMES}}),//TODO: lambda function
	streamingBuffer(resourceManager,
                TOTAL_BUFFER_FRAMES,
                this->numberOfLoopers * 3, //3 iterators per looper TODO: how much is needed?
                "Looper_buffer", "/mnt/sdcard/Samples/Loopers",
                availableLoopers),
	looperTriggerMode(LooperTriggerMode::OnBar){
	for(int i = 0; i < numberOfLoopers; i++){
		loopers.push_back(Looper(this, i));
	}
	streamingBuffer.initializeForLoopers(availableLoopers);
	streamingBuffer.printInfo();
}



bool Loopers::isRecording(){
	for(size_t i = 0; i < loopers.size(); i++){
		assert(loopers.size() > i);
		if(loopers.at(i).isRecording()){
			return true;
		}
	}
	return false;
}

bool Loopers::toggleRecord(int looperIndex){
	auto& looper = VEC_AT(loopers, looperIndex);
	if(!looper.isRecording() && this->isRecording()){
		rt_printf("already rec on other loop");
		return false;
	}
	return looper.toggleRecord();
}

bool Loopers::togglePlay(int looperIndex){
	if(looperIndex < 0 || static_cast<size_t>(looperIndex) >= loopers.size()){
		rt_printf("error, trying to togglePlay out of looper vector range\n");
		throw std::runtime_error("error, trying to togglePlay out of looper vector range");
	}
	assert(loopers.size() > static_cast<size_t>(looperIndex));
	return loopers.at(looperIndex).togglePlay();
	return false;
}



//TODO: each looper get its own gain
float Loopers::process(float inFrame){
	float mixedFrame = 0.0f;
	for(auto& looper: loopers){
		mixedFrame += looper.process(inFrame);
	}
	return mixedFrame;
}






/*
NOT READY YET

bool Loopers::newLooper(int loopLengthInFrames, int looperIndex){
	assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
	auto& looper = loopers.at(looperIndex);
	if(!looper.isEmpty()) return false;
	looper.setLoopLengthInFrames(loopLengthInFrames);

	looper.setStart(bufferWriteIndex);
	bufferWriteIndex += loopLengthInFrames;
	
	if(bufferWriteIndex > TOTAL_BUFFER_FRAMES){
		//TODO: rather do this differently so that if someone plays a long set they dont lose everything?
		rt_printf("ERROR: exceeded total loop buffer size\n");
		throw std::runtime_error("exceeded total loop buffer size");
	}
	
	return true;
}
*/












/*


OLD VERSION:
//TODO: deal with case where loopLengthInFrames is not 0 modulo blockSize !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void Looper::processBlockwise(std::vector<float>& blockFrames){
	//TODO: this seems a little stupid. while the looper is erasing it cannot be used.
	if(framesLeftToErase > 0){ 
		framesLeftToErase -= blockSize;
		for(int i = 0; i < blockSize; i++){
			const int absoluteIndex = startIndexInBuffer + position + i;
			assert(absoluteIndex >= 0 && buffer.size() > static_cast<size_t>(absoluteIndex));
			buffer.at(absoluteIndex) = 0.0f;
		}
		position += blockSize;
		return;
	}
	
	if(this->isRecording()){
		//TODO: make some way of making old recordings more quiet and test: i think it would be enough to do: frames *= 0.9 and then frames += newFrames. however when one stops the rec then i 
		//would have a jump where before the jump i have *=0.9 and after i dont. so i guess whenever i press record, it should immediately make the whole loop *=0.9
		for(int i = 0; i < blockSize; i++){
			const int absoluteIndex = startIndexInBuffer + position + i;
			assert(absoluteIndex >= 0 && buffer.size() > static_cast<size_t>(absoluteIndex));
			assert(blockFrames.size() > static_cast<size_t>(i));
			buffer.at(absoluteIndex) += blockFrames.at(i);
		}
	}
	if(playing){
		for(int i = 0; i < blockSize; i++){
			const int absoluteIndex = startIndexInBuffer + position + i;
			assert(blockFrames.size() > static_cast<size_t>(i));
			assert(absoluteIndex >= 0 && buffer.size() > static_cast<size_t>(absoluteIndex));
			blockFrames.at(i) += buffer.at(absoluteIndex);
		}
	}

	position += blockSize;
	if(position >= loopLengthInFrames){
		//TODO: guard this for the case that the looplength is not 0 mod 16
		position -= loopLengthInFrames;
	}
}




void Loopers::processBlockwise(std::vector<float>& blockFrames){
	for(auto& looper: loopers){
		looper.processBlockwise(blockFrames);
	}
}
*/

