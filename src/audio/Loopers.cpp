#include <vector>
#include <stdexcept>
#include <cassert>
//compiel
#include "../../include/audio/Loopers.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/general/Metronome.h"
#include "../../include/audio/Mixer.h"
#include "../../include/hardwareInterfaces/LooperLights.h"

static constexpr float LOOPER_OVERDUB_DECAY = 0.9f;

Looper::Looper(Loopers& parent, int looperIndex)
	: parent(parent),
	looperIndex(looperIndex),
	state(parent.looperLights, looperIndex) {}

LooperState::LooperState(LooperLights& looperLights, int looperIndex)
	: looperLights(looperLights), looperIndex(looperIndex)
{}

void LooperState::setPlaying(bool playing){
	this->playing = playing;
	if(playing){
		looperLights.setLight(LooperLightMessage::PlayingOn, looperIndex);
	} else {
		looperLights.setLight(LooperLightMessage::PlayingOff, looperIndex);
	}
}

void LooperState::setRecording(bool recording){
	this->recording = recording;
	if(recording){
		looperLights.setLight(LooperLightMessage::RecordingOn, looperIndex);
	} else {
		looperLights.setLight(LooperLightMessage::RecordingOff, looperIndex);
	}
}

void LooperState::setWaitingForBarStart(bool waitingForBarStart){
	this->waitingForBarStart = waitingForBarStart;
	if(waitingForBarStart){
		looperLights.setLight(LooperLightMessage::WaitingForBarStart, looperIndex);
	}
	//No else path. I think all possible cases do what they should this way
}

float Looper::getProgress(){
	if(loopLengthInFrames == 0) return 0.0f;
	return (float)position / (float)loopLengthInFrames;
}

bool Looper::toggleRecord() {
	if(state.isWaitingForBarStart()){
		rt_printf("still waiting for bar start, cannot toggle record\n");
		return false;
	}
	//First recording path
	if(loopLengthInFrames == 0){
		if(parent.looperTriggerMode == LooperTriggerMode::Free){
			if(state.isRecording()){
				rt_printf("stopping recording instantly\n");
				setNewLoopLength = true;
				state.setRecording(false);
				if(parent.autoplay) state.setPlaying(true);
				//TODO: option to keep overdubbing
				return false;
			} else {
				baseIteratorPtr = &parent.streamingBuffer.begin({0, looperIndex}, SBIType::Write);
				undoIteratorPtr = &parent.streamingBuffer.begin({1, looperIndex}, SBIType::Write);
				rt_printf("starting recording instantly\n");
				state.setRecording(true);
				return true;
			}
		} else if (parent.looperTriggerMode == LooperTriggerMode::OnBar){
			if(state.isRecording()){
				rt_printf("waiting for bar start to stop recording\n");
			} else {
				baseIteratorPtr = &parent.streamingBuffer.begin({0, looperIndex}, SBIType::Write);
				undoIteratorPtr = &parent.streamingBuffer.begin({1, looperIndex}, SBIType::Write);
				rt_printf("waiting for bar start to start recording\n");
			}
			state.setWaitingForBarStart(true);
			return false;
		}
	} else { // normal path
		if(!state.isRecording()){//starting record on existing loop -> passt down current undo content
			passDownFramesLeft = loopLengthInFrames;
		}
		state.setRecording(!state.isRecording());
		return state.isRecording();
	}
	//TODO: control when to flush.
	return false;
}

bool Looper::togglePlay() {
	state.setPlaying(!state.isPlaying());
	return state.isPlaying();
}

void Looper::undo(){
	if(loopLengthInFrames == 0){
		rt_printf("cannot undo when no loop is recorded\n");
		return;
	}
	rt_printf("undoing\n");
	undoFramesLeft = loopLengthInFrames;
}

void Looper::erase(){
	if(loopLengthInFrames == 0){
		rt_printf("cannot erase when no loop is recorded\n");
		return;
	}
	rt_printf("erasing\n");
	eraseFramesLeft = loopLengthInFrames;
}

//TODO: make this less complicated
float Looper::process(float inFrame){
	if(state.isWaitingForBarStart()){
		if(parent.metronome.getBeatsElapsed() == 0 && parent.metronome.getFrameCounter() == 0){
			rt_printf("bar started,");
			state.setWaitingForBarStart(false);
			if(state.isRecording()){
				rt_printf("setting loop length to %d frames\n", position);
				setNewLoopLength = true;
				//TODO: autodub option. also doing this twice i think
				state.setRecording(false);
				if(parent.autoplay){
					state.setPlaying(true);
				}
			} else {
				rt_printf("starting recording\n");
				state.setRecording(true);
			}
		}
		if(!state.isRecording()){
			return 0.0f;
		}
	}

	if(baseIteratorPtr != nullptr && undoIteratorPtr != nullptr){
		StreamingBufferIterator& baseIterator = *baseIteratorPtr;
		StreamingBufferIterator& undoIterator = *undoIteratorPtr;
		//While first recording path
		if(loopLengthInFrames == 0){
			*baseIterator = 0.0f; //setting baseIterator to 0 while recording first time
			*undoIterator = 0.0f;
			if(state.isRecording()){
				*undoIterator = inFrame;
			}
			if(setNewLoopLength){
				loopLengthInFrames = position;
				printf("set loop length to %d frames with position %d\n", loopLengthInFrames, position);
				parent.availableLoopers[{0, looperIndex}] = loopLengthInFrames;
				parent.availableLoopers[{1, looperIndex}] = loopLengthInFrames;
				*baseIterator = END_OF_SAMPLE;
				*undoIterator = END_OF_SAMPLE;
				setNewLoopLength = false;
				baseIterator.rewind();
				undoIterator.rewind();
				position = 0;
				return *undoIterator;
			}
			baseIterator++;
			undoIterator++;
			position++;
			return 0.0f;
		}

		//Normal path
		if(position >= loopLengthInFrames){
			baseIterator.rewind();
			undoIterator.rewind();
			position = 0;
		}
		if(passDownFramesLeft > 0){
			*baseIterator = *baseIterator + *undoIterator;
			*undoIterator = 0.0f;
			passDownFramesLeft--;
		}
		if(undoFramesLeft > 0){
			*undoIterator = 0.0f;
			undoFramesLeft--;
		}
		if(eraseFramesLeft > 0){
			*baseIterator = 0.0f;
			*undoIterator = 0.0f;
			eraseFramesLeft--;
		}
		if(overdubDecayFramesLeft > 0){
			*undoIterator = (*undoIterator) * LOOPER_OVERDUB_DECAY;//TODO: maybe better only on base iterator?
			*baseIterator = (*baseIterator) * LOOPER_OVERDUB_DECAY;
			overdubDecayFramesLeft--;
		}
		float outFrame = *baseIterator + *undoIterator;
		if(state.isRecording()){
			*undoIterator = *undoIterator  + inFrame;
			if(inFrame > 0.1f && overdubDecayFramesLeft <= 0){
				overdubDecayFramesLeft = loopLengthInFrames; //start decay because something was overdubbed
			}
		}
		baseIterator++;
		undoIterator++;
		position++;
		if(state.isPlaying()){
			return outFrame;
		}
		return 0.0f;
	}

	return 0.0f;
}




Loopers::Loopers(ResourceManager& resourceManager):
	resourceManager(resourceManager),
	metronome(resourceManager.getMetronome()),
	mixer(resourceManager.getMixer()),
	looperLights(resourceManager.getLooperLights()),
	numberOfLoopers(resourceManager.getDeviceMap().initialLooperNumber),//TODO: get this from device map or other
	availableLoopers([this]{
		std::unordered_map<SampleIdentifier, size_t> map;
		map.reserve(static_cast<size_t>(numberOfLoopers));
		for(int i = 0; i < numberOfLoopers; ++i){//base samples
			map.emplace(SampleIdentifier{0, i}, TOTAL_LOOPER_BUFFER_FRAMES);
		}
		for(int i = 0; i < numberOfLoopers; ++i){//undo samples
			map.emplace(SampleIdentifier{1, i}, TOTAL_LOOPER_BUFFER_FRAMES);
		}
		return map;
	}()),
	streamingBuffer(resourceManager,
                TOTAL_LOOPER_BUFFER_FRAMES,
                this->numberOfLoopers * 4, //4 iterators per looper TODO: how much is needed?
                "Looper_buffer", "/mnt/sdcard/Samples/Loopers",
                availableLoopers),
	looperTriggerMode(LooperTriggerMode::OnBar){
	for(int i = 0; i < numberOfLoopers; i++){
		loopers.push_back(Looper(*this, i));
	}
	streamingBuffer.initializeForLoopers(availableLoopers);
	streamingBuffer.printInfo();
}

int Loopers::size(){
	return loopers.size();
}

void Loopers::setLooperTriggerMode(LooperTriggerMode mode){
	looperTriggerMode = mode;
}

float Loopers::getProgress(int looperIndex){
	if(looperIndex < 0 || static_cast<size_t>(looperIndex) >= loopers.size()){
		looperIndex = editableLooperIndex;
	}
	Looper& looper = VEC_AT(loopers, looperIndex);
	return looper.getProgress();
}

int Loopers::getProgressInBars(int looperIndex){
	if(looperIndex < 0 || static_cast<size_t>(looperIndex) >= loopers.size()){
		looperIndex = editableLooperIndex;
	}
	Looper& looper = VEC_AT(loopers, looperIndex);
	float progress = getProgress(looperIndex) - 0.001;//TODO: remove magic number
	if(progress < 0.0f) progress = 1.0f + progress; //wrap around;
	int barsCompleted = (float)getLengthInBars(looperIndex) * progress; 
	//rt_printf("looper %d progress in bars: %d / %d with percentage progress %f\n", looperIndex, barsCompleted, getLengthInBars(looperIndex), looper.getProgress());
	return barsCompleted;
}

int Loopers::getLengthInBars(int looperIndex){
	if(looperIndex < 0 || static_cast<size_t>(looperIndex) >= loopers.size()){
		looperIndex = editableLooperIndex;
	}
	Looper& looper = VEC_AT(loopers, looperIndex);
	size_t loopLengthInFrames = looper.loopLengthInFrames;
	int framesPerBar = metronome.getFramesPerBeat() * metronome.getBeatsPerBar();
	int bars = loopLengthInFrames / framesPerBar;
	rt_printf("looper %d length in bars: %d, length in frames: %zu\n", looperIndex, bars, loopLengthInFrames);
	if(loopLengthInFrames % framesPerBar != 0){ //TODO: remove magic number
		bars += 1; //partial bar counts as full bar
	}
	return bars;
}



	
bool Loopers::isRecording(){
	for(size_t i = 0; i < loopers.size(); i++){
		assert(loopers.size() > i);
		if(loopers.at(i).state.isRecording()){
			return true;
		}
	}
	return false;
}

bool Loopers::isRecording(int looperIndex){
	Looper& looper = VEC_AT(loopers, looperIndex);
	return looper.state.isRecording();
}

bool Loopers::isWaitingForBarStart(int looperIndex){
	Looper& looper = VEC_AT(loopers, looperIndex);
	return looper.state.isWaitingForBarStart();
}

bool Loopers::isEmpty(int looperIndex){
	Looper& looper = VEC_AT(loopers, looperIndex);
	return looper.loopLengthInFrames == 0 && !looper.state.isRecording();
}

bool Loopers::toggleRecord(int looperIndex){
	editableLooperIndex = looperIndex;
	if(looperIndex < 0 || static_cast<size_t>(looperIndex) >= loopers.size()){
		rt_printf("error, trying to toggleRecord out of looper vector range\n");
		throw std::runtime_error("error, trying to toggleRecord out of looper vector range");
	}
	auto& looper = VEC_AT(loopers, looperIndex);
	if(!looper.state.isRecording() && this->isRecording()){
		rt_printf("already rec on other loop");
		return false;
	}
	return looper.toggleRecord();
}

bool Loopers::togglePlay(int looperIndex){
	editableLooperIndex = looperIndex;
	if(looperIndex < 0 || static_cast<size_t>(looperIndex) >= loopers.size()){
		rt_printf("error, trying to togglePlay out of looper vector range\n");
		throw std::runtime_error("error, trying to togglePlay out of looper vector range");
	}
	assert(loopers.size() > static_cast<size_t>(looperIndex));
	bool playing = loopers.at(looperIndex).togglePlay();
	return playing;
}

void Loopers::setPlaying(int looperIndex, bool playing){
	Looper& looper = VEC_AT(loopers, looperIndex);
	if(looper.state.isPlaying() != playing)
		looper.togglePlay();
}


float Loopers::process(float inFrame){
	float mixedFrame = 0.0f;
	for(auto& looper: loopers){
		mixedFrame += looper.process(inFrame) * mixer.getLooperGain(looper.looperIndex);
	}
	return mixedFrame;
}

void Loopers::undo(int looperIndex){
	Looper& looper = VEC_AT(loopers, looperIndex);
	looper.undo();
}


void Loopers::undo(){
	undo(editableLooperIndex);
}

void Loopers::erase(int looperIndex){
	Looper& looper = VEC_AT(loopers, looperIndex);
	looper.erase();
}

void Loopers::erase(){
	erase(editableLooperIndex);
}

bool Loopers::setLengthInBars(int looperIndex, int bars){
	if(!isEmpty(looperIndex))
		return false;
	Looper& looper = VEC_AT(loopers, looperIndex);
	int lengthInFrames = bars * metronome.getFramesPerBeat() * metronome.getBeatsPerBar();
	looper.loopLengthInFrames = lengthInFrames;
	looper.baseIteratorPtr = &streamingBuffer.begin({0, looperIndex}, SBIType::Write);
	looper.undoIteratorPtr = &streamingBuffer.begin({1, looperIndex}, SBIType::Write);
	availableLoopers[{0, looperIndex}] = looper.loopLengthInFrames;
	availableLoopers[{1, looperIndex}] = looper.loopLengthInFrames;
	//set both samples to 0.0 on first pass
	looper.framesLeftToErase = looper.loopLengthInFrames;
	return true;
}

bool Loopers::setLengthInBars(int bars){
	return setLengthInBars(editableLooperIndex, bars);
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
	
	if(bufferWriteIndex > TOTAL_LOOPER_BUFFER_FRAMES){
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
