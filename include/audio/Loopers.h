#pragma once
#include <vector>
#include <stdexcept>
#include <cassert>
//compile
#include "../streamingBuffer/StreamingBuffer.h"
//I had a version where the loopers were not stored in one big buffer and then I needed a thread to initialize the looper with the big vector, the function doing this is also in midi_keyboard_6_16_25

constexpr size_t TOTAL_LOOPER_BUFFER_FRAMES = 44100 * 60 * 2; // 2 minutes of space

class ResourceManager; //forward declaration
class Loopers;
class Metronome;
class Mixer;
class LooperLights;
enum class LooperLightMessage;

/*
TODO: it would be better to organise this way: the 0th loop is 5 bars long, so it has
	5 samples in the streamingbuffer called {0,0}, {0,1}, ...
	This way we only need to have 1 bar per looper in memory and have basically infinite looperspace
	Additinoally make the possibility to have an undo depth > 1. For that a possibility to make it would be:
	To have an undo offset e.g. UNDO_OFFSET = 1000 and then the first undo layer would be
	{1000,0}, {1000,1}, ...
	and the second layer
	{2000,0}, {2000,1}, ...
	and so on
	Then i would also not have to set the sample length to a huge number but instead already
	prepare hundreds of samples (e.g. 50 bars for each looper) and if i need
	more bars or loopers make them on the fly, (however that should be avoided
	since then we would need mutate the datastructure.)
*/


//The following class does the following:
//Whenever a looper changes state, it updates the lights accordingly
class LooperState{
public:
	LooperState(LooperLights& looperLights, int looperIndex);
	void setPlaying(bool playing);
	void setRecording(bool recording);
	void setWaitingForBarStart(bool waitingForBarStart);

	bool isRecording(){return recording;}
	bool isPlaying(){return playing;}
	bool isWaitingForBarStart(){return waitingForBarStart;}

private:
	bool playing = false;
	bool recording = false;
	bool waitingForBarStart = false;
	LooperLights& looperLights;
	int looperIndex;
};


class Looper {
public:
	Looper(Loopers& parent, int looperIndex);
					
	float getProgress();
				
	float process(float inFrame);

	bool toggleRecord();
	
	bool togglePlay();

	void undo();

	void erase();
	
private:
	friend class Loopers;
	Loopers& parent;
	StreamingBufferIterator* baseIteratorPtr = nullptr;
	StreamingBufferIterator* undoIteratorPtr = nullptr;
	int looperIndex;
	int position = 0;
	int loopLengthInFrames = 0;
	LooperState state;
	int framesLeftToErase = 0;
	bool setNewLoopLength = false;

	int passDownFramesLeft = 0;
	int undoFramesLeft = 0;
	int eraseFramesLeft = 0;
	int overdubDecayFramesLeft = 0;
};


enum class LooperTriggerMode {
	OnBar,
	Free,
	COUNT
};


class Loopers{
public:
	Loopers(ResourceManager& resourceManager);

	int size();
	
	LooperTriggerMode getLooperTriggerMode(){return looperTriggerMode;}

	void setLooperTriggerMode(LooperTriggerMode mode);
	
	float getProgress(int looperIndex = -1);
	int getProgressInBars(int looperIndex = -1);

	int getLengthInBars(int looperIndex = -1);

	int getEditableLooperIndex(){return editableLooperIndex;}

	void setEditableLooperIndex(int value){editableLooperIndex = value;}

	float process(float inFrame);
	
	bool isRecording();
	
	bool isRecording(int looperIndex);

	bool isWaitingForBarStart(int looperIndex);

	bool isEmpty(int looperIndex);
	
	bool toggleRecord(int looperIndex);
	
	bool togglePlay(int looperIndex);

	void setPlaying(int looperIndex, bool playing);

	void undo(int looperIndex);
	void undo();

	void erase(int looperIndex);
	void erase();

	bool setLengthInBars(int looperIndex, int bars);
	bool setLengthInBars(int bars);

private:
	friend class Looper;
	ResourceManager& resourceManager;
	Metronome& metronome;
	Mixer& mixer;
	LooperLights& looperLights;
	size_t bufferWriteIndex = 0;
	int numberOfLoopers;
	int editableLooperIndex = 0;
	std::unordered_map<SampleIdentifier, size_t> availableLoopers;
	StreamingBuffer streamingBuffer;
	std::vector<Looper> loopers;
	LooperTriggerMode looperTriggerMode;
	bool autoplay = true;
};
