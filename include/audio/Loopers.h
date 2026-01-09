#pragma once
#include <vector>
#include <stdexcept>
#include <cassert>
//compile
#include "../general/ResourceManager.h"
//I had a version where the loopers were not stored in one big buffer and then I needed a thread to initialize the looper with the big vector, the function doing this is also in midi_keyboard_6_16_25

constexpr size_t TOTAL_BUFFER_FRAMES = 44100 * 60 * 2; // 2 minutes of space


class Looper {
public:
	Looper(std::vector<float>& buffer, int blockSize):  blockSize(blockSize), buffer(buffer) {loopLengthInFrames = 0;}
	
	void setStart(int startIndexInBuffer){this->startIndexInBuffer = startIndexInBuffer;}
	
	void setLoopLengthInFrames(int loopLengthInFrames){this->loopLengthInFrames = loopLengthInFrames;}
	
	int getLoopLengthInFrames(){return loopLengthInFrames;}
	
	int getPosition(){return position;}
	
	float getProgress();
	
	bool isEmpty(){return loopLengthInFrames == 0 && !recording;}
	
	bool isRecording(){return recording;}
	
	bool isPlaying(){return playing;}
	
	void processBlockwise(std::vector<float>& blockFrames);

	float process() {return 0.0f;}

	bool toggleRecord();
	
	bool togglePlay();
	
	//TODO: The erase as it is implemented seems stupid, do differently!!!!!!!!!
	void eraseLoop(){framesLeftToErase = loopLengthInFrames;}
	
private:
	int blockSize = 16; //TODO: make blocksize come from resourceManager
	int startIndexInBuffer = -1;
	std::vector<float>& buffer;
	int position = 0;
	int loopLengthInFrames = 0;
	bool playing = false;
	bool recording = false;
	int framesLeftToErase = 0;
};



class Loopers{
public:
	Loopers(ResourceManager& resourceManager);
	
	bool newLooper(int loopLengthInFrames, int looperIndex);
	
	int getNrLoopers(){return loopers.size();}

	bool isPlaying(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		return loopers.at(looperIndex).isPlaying();
	}
	
	bool isEmpty(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		return loopers.at(looperIndex).isEmpty();
	}
	
	float getProgress(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		return loopers.at(looperIndex).getProgress();
	}

	void processBlockwise(std::vector<float>& blockFrames);

	float process() {return 0.0f;}
	
	bool isRecording();
	
	bool isRecording(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		return loopers.at(looperIndex).isRecording();
	}
	
	bool toggleRecord(int looperIndex);
	
	bool togglePlay(int looperIndex);
	
	void eraseLoop(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		loopers.at(looperIndex).eraseLoop();
	}
	
private:
	ResourceManager& resourceManager;
	int blockSize;
	size_t bufferWriteIndex = 0;
	std::vector<float> bigLooperBuffer;
	std::vector<Looper> loopers;
	int numberOfLoopers;
};
