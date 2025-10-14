#include <vector>
#include <stdexcept>

#include "../include/Loopers.h"
#include "../include/ResourceManager.h"


	
float Looper::getProgress(){
	if(loopLengthInFrames == 0) return 0.0f;
	return (float)position / (float)loopLengthInFrames;
}


//TODO: deal with case where loopLengthInFrames is not 0 modulo blockSize !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void Looper::processBlockwise(std::vector<float>& blockFrames){
	//TODO: this seems a little stupid. while the looper is erasing it cannot be used.
	if(framesLeftToErase > 0){ 
		framesLeftToErase -= blockSize;
		for(int i = 0; i < blockSize; i++){
			buffer.at(startIndexInBuffer + position + i) = 0.0f;
		}
		position += blockSize;
		return;
	}
	
	if(this->isRecording()){
		//TODO: make some way of making old recordings more quiet and test: i think it would be enough to do: frames *= 0.9 and then frames += newFrames. however when one stops the rec then i 
		//would have a jump where before the jump i have *=0.9 and after i dont. so i guess whenever i press record, it should immediately make the whole loop *=0.9
		for(int i = 0; i < blockSize; i++)
			buffer.at(startIndexInBuffer + position + i) += blockFrames.at(i);
	}
	if(playing){
		for(int i = 0; i < blockSize; i++)
			blockFrames.at(i) += buffer.at(startIndexInBuffer + position + i);
	}

	position += blockSize;
	if(position >= loopLengthInFrames){
		//TODO: guard this for the case that the looplength is not 0 mod 16
		position -= loopLengthInFrames;
	}
}


bool Looper::toggleRecord() {
	if(loopLengthInFrames == 0 && recording){
		loopLengthInFrames = position; //If it is recording for the first time the length is set on the 2nd toggle
	}
	recording = !recording;
	return recording;
}

bool Looper::togglePlay() {
	playing = !playing;
	return playing;
}





Loopers::Loopers(ResourceManager* resourceManager){
	blockSize = resourceManager->audioFramesPerBlock;
	bigLooperBuffer = std::vector<float>(TOTAL_BUFFER_FRAMES, 0.0f);
	numberOfLoopers = 8; //TODO make this come from deviceMap
	for(int i = 0; i < numberOfLoopers; i++){
		loopers.push_back(Looper(bigLooperBuffer, blockSize));
	}
}

bool Loopers::newLooper(int loopLengthInFrames, int looperIndex){
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

void Loopers::processBlockwise(std::vector<float>& blockFrames){
	for(auto& looper: loopers){
		looper.processBlockwise(blockFrames);
	}
}

bool Loopers::isRecording(){
	for(int i = 0; i < loopers.size(); i++){
		if(loopers.at(i).isRecording()){
			return true;
		}
	}
	return false;
}

bool Loopers::toggleRecord(int looperIndex){
	if(!loopers.at(looperIndex).isRecording()){
		if(this->isRecording()){
			rt_printf("already rec on other loop");
			return false;
		}
	}
	if(looperIndex < loopers.size()){
		auto& looper = loopers.at(looperIndex);
		if (looper.getLoopLengthInFrames() == 0){
			rt_printf("setting on first recording looper, setting it's length\n");
			int position = looper.getPosition();
			bufferWriteIndex += position;
			if(bufferWriteIndex > TOTAL_BUFFER_FRAMES){
				rt_printf("exceeded total loop buffer size\n");
				throw std::runtime_error("exceeded total loop buffer size");
			}
		}
		return looper.toggleRecord();
	} else {
		rt_printf("error, trying to toggle record out of looper vector range\n");
		throw std::runtime_error("error, trying to toggle record out of looper vector range");
		return false;
	}
	return false;
}

bool Loopers::togglePlay(int looperIndex){
	if(looperIndex < loopers.size()){
		return loopers.at(looperIndex).togglePlay();
	} else {
		rt_printf("error, trying to togglePlay out of looper vector range\n");
		throw std::runtime_error("error, trying to togglePlay out of looper vector range");
		return false;
	}
}





