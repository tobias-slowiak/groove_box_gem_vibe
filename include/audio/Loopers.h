#pragma once
#include <vector>
#include <stdexcept>
#include <cassert>
//compile
#include "../streamingBuffer/StreamingBuffer.h"
//I had a version where the loopers were not stored in one big buffer and then I needed a thread to initialize the looper with the big vector, the function doing this is also in midi_keyboard_6_16_25

constexpr size_t TOTAL_BUFFER_FRAMES = 44100 * 60 * 2; // 2 minutes of space

class ResourceManager; //forward declaration
class Loopers;
class Metronome;

class Looper {
public:
	Looper(Loopers* parentPtr, int looperIndex):  parentPtr(parentPtr), looperIndex(looperIndex){}
		
	void setLoopLengthInFrames(int loopLengthInFrames){this->loopLengthInFrames = loopLengthInFrames;}
	
	int getLoopLengthInFrames(){return loopLengthInFrames;}
	
	int getPosition(){return position;}
	
	float getProgress();
	
	bool isEmpty(){return loopLengthInFrames == 0 && !recording;}
	
	bool isRecording(){return recording;}
	
	bool isPlaying(){return playing;}

	float process(float inFrame);

	bool toggleRecord();
	
	bool togglePlay();
	
private:
	friend class Loopers;
	Loopers* parentPtr;
	StreamingBufferIterator* iteratorPtr = nullptr;
	int looperIndex;
	int position = 0;
	int loopLengthInFrames = 0;
	bool playing = false;
	bool recording = false;
	int framesLeftToErase = 0;
	bool setNewLoopLength = false;
	bool waitingForBarStart = false;
};


enum class LooperTriggerMode {
	OnBar,
	Free,
	COUNT
};


class Loopers{
public:
	Loopers(ResourceManager& resourceManager);
	
	
	int getNrLoopers(){return loopers.size();}

	LooperTriggerMode getLooperTriggerMode(){return looperTriggerMode;}
	void looperTriggerModeToggle(){
		int mode = static_cast<int>(looperTriggerMode);
		mode = (mode + 1) % static_cast<int>(LooperTriggerMode::COUNT);
		looperTriggerMode = static_cast<LooperTriggerMode>(mode);
	}

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

	float process(float inFrame);
	
	bool isRecording();
	
	bool isRecording(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		return loopers.at(looperIndex).isRecording();
	}

	bool isWaitingForBarStart(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		return loopers.at(looperIndex).waitingForBarStart;
	}
	
	bool toggleRecord(int looperIndex);
	
	bool togglePlay(int looperIndex);
	
	/*
	NOT READY YET

		bool newLooper(int loopLengthInFrames, int looperIndex);


	void eraseLoop(int looperIndex){
		assert(looperIndex >= 0 && loopers.size() > static_cast<size_t>(looperIndex));
		loopers.at(looperIndex).eraseLoop();
	}
		*/

	//OLD     void processBlockwise(std::vector<float>& blockFrames);
	
private:
	friend class Looper;
	ResourceManager& resourceManager;
	Metronome& metronome;
	size_t bufferWriteIndex = 0;
	int numberOfLoopers;
	std::unordered_map<SampleIdentifier, size_t> availableLoopers;
	StreamingBuffer streamingBuffer;
	std::vector<Looper> loopers;
	LooperTriggerMode looperTriggerMode;
	bool autoplay = true;
};
