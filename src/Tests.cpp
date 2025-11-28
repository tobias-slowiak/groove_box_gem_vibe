#include <Bela.h>

#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>

#include "../include/ModeManager.h"
#include "../include/ResourceManager.h"
#include "../include/StreamingBuffer.h"
#include "../include/IDisplayContext.h"
#include "../include/BelaInterface.h"
#include "../include/Controller.h"
#include "../include/BasicUtilities.h"
#include "../include/Loopers.h"
#include "../include/Samplers.h"
#include "../include/Voices.h"
#include "../include/SamplePack.h"

void ModeManager::renderBelaInterfaceTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	static int blocksElapsed = 0;
	blocksElapsed++;

	resourceManager->getDisplayContext()->processBlockwise();

	BelaInterface* interface = resourceManager->getBelaInterface();
	interface->processBlockwise();
	while(interface->numAvailableMessages() > 0){
		InterfaceMessage msg = interface->getNextInterfaceMessage();
		msg.prettyPrint();
		if(msg.type == InterfaceMessageType::ButtonPressed && msg.id == 0){
			currentTestDone = true;
			blocksElapsed = 0;
		}
	}
	
	if(blocksElapsed%(2 * resourceManager->audioFramesPerSecond / resourceManager->audioFramesPerBlock) == 0){
		interface->printPotentiometerValues();
	}
	if(secondsElapsed(blocksElapsed, 10) ){
		currentTestDone = true;
		blocksElapsed = 0;
	}
	
}

void ModeManager::renderDisplayContextTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	static int blocksElapsed = 0;
	blocksElapsed++;

	IDisplayContext* display = resourceManager->getDisplayContext();
	display->processBlockwise();
	if(secondsElapsed(blocksElapsed, 1)){
		display->setLines(0, 0, "hello");
	}
	if(secondsElapsed(blocksElapsed, 3) ){
		display->setLines(0, 0, "hello", "how", "are", "you? :)");
	}
	if(secondsElapsed(blocksElapsed, 5) ){
		display->setLines(1, 0, "on", "the", "right", "display!");
	}
	if(secondsElapsed(blocksElapsed, 7) ){
		display->setLines(0, 0, "progress", "bar:", " ", " ");//send spaces if you want to reset the whole display
		display->setProgress(0, 0.5);
	}
	if(secondsElapsed(blocksElapsed, 9) ){
		display->setProgress(0, 0.6);
	}
	if(secondsElapsed(blocksElapsed, 11)){
		display->setProgress(0, 0.7);
	}
	
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {

	}
	if(secondsElapsed(blocksElapsed, 12) ){
		currentTestDone = true;
		blocksElapsed = 0;
		display->setProgress(0, -1.0);
		display->setLines(1, 0, "", "", "", "");
	}
}
void ModeManager::renderVoicesTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	static int blocksElapsed = 0;
	blocksElapsed++;
	static int note = 21;
	static int velocity = 1;
	SamplePack* sp = resourceManager->getKeyInstrumentSamplePack();
	StreamingBuffer* sb = &(sp->streamingBuffer);
	Voices* voices = resourceManager->getVoices();

	if(blocksElapsed == 1){
		sb->printInfo();
		sb->requestSample({21,1});
	}

	if(blocksElapsed == 2) {
		sb->printInfo();
		rt_printf("starting playback of testSample on repeat\n");
		voices->triggerVoice(sb, note, velocity, 0, 0, 1, 0, 1, 1, true);
	}
	if(secondsElapsed(blocksElapsed, 2) ){
		rt_printf("stopping playback of testSample on repeat\n");
		resourceManager->getVoices()->triggerOff(note);
	}
	if(secondsElapsed(blocksElapsed, 3) ){
		rt_printf("starting playback of testSample with long adsr on repeat\n");
		resourceManager->getVoices()->triggerVoice(sb, note+3, velocity+5, 0.5, 0.5, 0.5, 0.5, 1, 1, true);
	}
	if(secondsElapsed(blocksElapsed, 5) ){
		rt_printf("stopping playback of first testSample on repeat\n");
		resourceManager->getVoices()->triggerOff(note+3);
	}
	if(secondsElapsed(blocksElapsed, 6)){
		rt_printf("now creating a new voice every 0.1 s with increasing playbackrate\n");
	}
	if(blocksElapsed > 6 * resourceManager->blocksPerSecond){
		if(blocksElapsed % (int)(0.1 * resourceManager->blocksPerSecond) == 0){
			rt_printf("triggering note %d\n", note);
			note += 3;
			velocity++;
			if(note > 33){
				note = 21; //dont load too many samples to avoid saturation of streaming buffer
				velocity = 1;
			}
			resourceManager->getVoices()->triggerVoice(sb, note, velocity, 0.01, 0.1, 0.7, 0.1, 0.5, 1, true);
		}
	}
	if(secondsElapsed(blocksElapsed, 9.2) ){
		rt_printf("\n\n\n\n\n\nshould be saturated now\n\n\n\n\n\n\n");
	}
	
	
	sb->processBlockwise();
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = voices->process();
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch,  frame);
        }
	}
		
	if(secondsElapsed(blocksElapsed, 120) ){
		for(int i = 0; i <= 33; i++){
			resourceManager->getVoices()->triggerOff(i);
		}
		currentTestDone = true;
		blocksElapsed = 0;
	}
}

/*
//TODO: for these tests i need to incorporate that the voices class now works with streamingbuffer


void ModeManager::renderLoopersTest(BelaContext *context, ResourceManager* resourceManager){
	static int blocksElapsed = 0;
	blocksElapsed++;

	static std::vector<float> blockFrames(16,0.0f);
	static Loopers* loopers = resourceManager->getLoopers();
	static BelaInterface* interface = resourceManager->getBelaInterface();
	static float outputGain = 1.0;
	static float inputGain = 1.0;
	static std::vector<float>* testSample = resourceManager->getTestSampleVector();
	static int testSampleSize = testSample->size();
	static int testSampleIndex = 0;
	
	if(blocksElapsed == 1) loopers->newLooper(44100, 0);

	interface->processBlockwise();
	while(interface->numAvailableMessages() > 0){
		InterfaceMessage msg = interface->getNextInterfaceMessage();
		if(msg.type == InterfaceMessageType::PotSignal && msg.id == 0) outputGain = msg.value;
	}
	
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		testSampleIndex++;
		if(testSampleIndex >= testSampleSize) testSampleIndex = 0;
		assert(blockFrames.size() > n);
		blockFrames.at(n) = 0.0f;
		if(blocksElapsed < 3*44100/16){
			assert(blockFrames.size() > n);
			assert(testSample->size() > static_cast<size_t>(testSampleIndex));
			blockFrames.at(n) += testSample->at(testSampleIndex);
		}
		for(unsigned int ch = 0; ch < context->audioInChannels; ch++) {
            assert(blockFrames.size() > n);
            blockFrames.at(n) += audioRead(context, n, ch) * inputGain;
        }
	}
	if(secondsElapsed(blocksElapsed, 2)){
		rt_printf("starting record on looper 0\n");
		loopers->toggleRecord(0);
	}

	if(secondsElapsed(blocksElapsed, 5)){
		rt_printf("starting play on looper 0\n");
		loopers->togglePlay(0);
	}
	loopers->processBlockwise(blockFrames);
	
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		for(unsigned int ch = 0; ch < context->audioInChannels; ch++) {
            assert(blockFrames.size() > n);
            audioWrite(context, n, ch, outputGain * blockFrames.at(n));
        }
	}
	if(secondsElapsed(blocksElapsed, 10) ){
		currentTestDone = true;
		blocksElapsed = 0;
	}
}

void ModeManager::renderSamplersTest(BelaContext *context, ResourceManager* resourceManager){
	static int blocksElapsed = 0;
	blocksElapsed++;

	static int testSampleIndex = 0;
	Samplers* samplers = resourceManager->getSamplers();
	
	if(blocksElapsed == 1) {
		samplers->newSampler(1);
		samplers->newSampler(3);
	}
	if(secondsElapsed(blocksElapsed, 1)){
		rt_printf("starting record on sampler 0\n");
		samplers->startRecord(0);
	}
	if(secondsElapsed(blocksElapsed, 2) ){
		rt_printf("starting record on sampler 1\n");
		samplers->startRecord(1);
	}
	if(secondsElapsed(blocksElapsed, 3) ){
		rt_printf("starting playback on sampler 0 with length %f s\n", samplers->getSampleSlice(0, 0).second / (float)resourceManager->audioFramesPerSecond);
		resourceManager->getVoices()->triggerVoice(samplers->getSampleSlice(0, 0), 0);
	}
	if(secondsElapsed(blocksElapsed, 5) ){
		rt_printf("starting playback on sampler 1 with length %f s\n", samplers->getSampleSlice(1, 0).second / (float)resourceManager->audioFramesPerSecond);
		resourceManager->getVoices()->triggerVoice(samplers->getSampleSlice(1, 0), 1);
	}
	
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float inputFrame = resourceManager->getTestSampleVector()->at(testSampleIndex);
		testSampleIndex++;
		if(testSampleIndex >= resourceManager->getTestSample().second) testSampleIndex = 0;
		samplers->process(inputFrame);
		float frame = resourceManager->getVoices()->process();
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch, 0.1 * frame);
        }
	}
	if(secondsElapsed(blocksElapsed, 9) ){
		for(int i = 0; i <= 1; i++){
			resourceManager->getVoices()->triggerOff(i);
		}
		currentTestDone = true;
		blocksElapsed = 0;
	}
}
*/

void ModeManager::renderLoopersTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
}

void ModeManager::renderSamplersTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
}


void ModeManager::renderMidiTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	static int blocksElapsed = 0;
	blocksElapsed++;
	
	IMidi* keyMidi = resourceManager->getKeyMidi();
	IMidi* controlMidi = resourceManager->getControlMidi();
	
	while(keyMidi->getParser()->numAvailableMessages() > 0) {
        IMidiChannelMessage* message = keyMidi->getParser()->getNextChannelMessage();
        message->prettyPrint();
	}
	while(controlMidi->getParser()->numAvailableMessages() > 0){
		IMidiChannelMessage* message = controlMidi->getParser()->getNextChannelMessage();
		message->prettyPrint();
	}
	//TODO: change the ending and also make it testable without prints.
	if(secondsElapsed(blocksElapsed, 5) ){
		currentTestDone = true;
		blocksElapsed = 0;
	}
}

void ModeManager::renderControllerTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	static int blocksElapsed = 0;
	blocksElapsed++;

	Controller* controller = resourceManager->getController();
	
	controller->processBlockwise();
	
	if((blocksElapsed * resourceManager->blocksPerSecond)%2 == 0){
		//TODO: do something
	}
	if(secondsElapsed(blocksElapsed, 10) ){
		currentTestDone = true;
		blocksElapsed = 0;
	}

}

void ModeManager::renderSamplePackTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	static int blocksElapsed = 0;
	blocksElapsed++;
	static SamplePack* samplePack = resourceManager->getKeyInstrumentSamplePack();
	static Voices* voices = resourceManager->getVoices();
	/*
	if(secondsElapsed(blocksElapsed, 1)){
		std::string samplepart = "";
		std::vector<float>* sample;
		sample = samplePack->getSampleStart(75, 15);
		for(int i = 70; i <= 100; i++){
			samplepart += std::to_string(sample->at(i)) + " ";
		}
		rt_printf("sampleEntries: %s\n", samplepart.c_str());
		rt_printf("playing note 75 v 15\n");
		voices->triggerVoice(sample, 21, 0.01, 0.1, 0.7, 0.1, 1.0, 1, false);
	}
		*/
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = resourceManager->getVoices()->process();
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch, 0.1 * frame);
        }
	}

	if(secondsElapsed(blocksElapsed, 5) ){
		currentTestDone = true;
		blocksElapsed = 0;
	}
}












	/*
	Findings: the chunksize strongly influences the bandwidth. up to 200 ms the increase is
	steep, afterwards not so much. for 50 ms we get approx. 10 MB/s for the chunks,
	with 200ms weg et approximately 20 MB/s. theoretically it should be:
	the more BW the more chunks can be streamed at the same time
	with a linear relationship.
	Theoretically, if I dont have a thinking error, 20MB/s should make it
	possible to stream 110 samples at the same time (which is way too much)
	For example if we have 200ms then 1 sample is 35280 bytes.
	20 MB/s means 4MB/0.2s.
	this means i should be able to stream 4000000 / 35280 = 113 samples at the same time.
	However, in practice, i dont get these results at all. This test always fails with
	nrSamples > 20 no matter the chunksize. with 200 ms i can do the 20 samples,
	so i think 200ms is a good compromise for now.
	Note that here i can allocate the full BW for this one streaming buffer.
	in practice i would have multiple streaming buffers competing for BW.
	TODO: try with emmc instead of sd card.
	TODO: make this functional and then do it more parametrized so that i can 
		test different ms once then different nrSamples the next time and so on.
	TODO: try with better SD card.
	
	*/
void ModeManager::renderStreamingBandwidthTest(BelaContext *context, ResourceManager* resourceManager){
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	struct ConfigKey {
		int chunkMs = 0;
		int sampleCount = 0;
		bool operator<(const ConfigKey& other) const {
			if(chunkMs == other.chunkMs) {
				return sampleCount < other.sampleCount;
			}
			return chunkMs < other.chunkMs;
		}
	};

	struct RunningStats {
		int count = 0;
		double sum = 0.0;
		double sumSquares = 0.0;
		float mean = 0.0f;
		float stddev = 0.0f;

		void add(float value){
			count++;
			sum += value;
			sumSquares += static_cast<double>(value) * static_cast<double>(value);
		}

		void finalize(){
			if(count <= 0){
				mean = 0.0f;
				stddev = 0.0f;
				return;
			}
			mean = static_cast<float>(sum / static_cast<double>(count));
			if(count <= 1){
				stddev = 0.0f;
				return;
			}
			double variance = (sumSquares / static_cast<double>(count)) - (static_cast<double>(mean) * static_cast<double>(mean));
			if(variance < 0.0){
				variance = 0.0;
			}
			stddev = sqrtf(static_cast<float>(variance));
		}
	};

	struct ConfigResult {
		RunningStats start;
		RunningStats chunk;
	};

	enum class Phase {
		TriggerStart,
		AwaitStart,
		WaitBeforeChunk,
		TriggerChunks,
		AwaitChunks,
		WaitBeforeNextConfig,
		Complete
	};

	struct ExperimentState {
		bool initialized = false;
		bool complete = false;
		bool printed = false;
		Phase phase = Phase::TriggerStart;
		double phaseStartTime = 0.0;
		double cooldownEndTime = 0.0;
		bool waitingForStartFlagRise = false;
		bool waitingForChunkFlagRise = false;
		int runsTotal = 0;
		int currentRun = 0;
		size_t chunkIndex = 0;
		size_t sampleIndex = 0;
		float timeDelta = 0.0f;
		size_t availableStartCount = 0;
		size_t chunkSamples = 0;
		size_t chunkBytes = 0;
		size_t totalChunkSamples = 0;
		uint32_t targetStartJob = 0;
		uint32_t targetChunkJob = 0;
		ConfigKey currentKey;
		std::vector<int> chunkSizesMs;
		std::vector<int> sampleCounts;
		const std::vector<std::pair<int,int>>* availableSampleIds = nullptr;
		std::map<ConfigKey, ConfigResult> results;

		void resetTransient(){
			waitingForStartFlagRise = false;
			waitingForChunkFlagRise = false;
			totalChunkSamples = 0;
			phaseStartTime = 0.0;
			availableStartCount = 0;
			targetStartJob = 0;
			targetChunkJob = 0;
		}
		void resetState(){
			initialized = false;
			complete = false;
			printed = false;
			phase = Phase::TriggerStart;
			phaseStartTime = 0.0;
			cooldownEndTime = 0.0;
			waitingForStartFlagRise = false;
			waitingForChunkFlagRise = false;
			runsTotal = 0;
			currentRun = 0;
			chunkIndex = 0;
			sampleIndex = 0;
			timeDelta = 0.0f;
			availableStartCount = 0;
			chunkSamples = 0;
			chunkBytes = 0;
			totalChunkSamples = 0;
			targetStartJob = 0;
			targetChunkJob = 0;
			currentKey = {};
			chunkSizesMs.clear();
			sampleCounts.clear();
			results.clear();
			availableSampleIds = nullptr;
		}
	};

	static ExperimentState state;
	static int blocksElapsed = 0;
	blocksElapsed++;

	const std::string folderPath = "/mnt/sdcard/Samples/converted_new/";

	SamplePack* samplePack = resourceManager->getKeyInstrumentSamplePack();
	if(!samplePack){
		rt_printf("renderStreamingBandwidthTest: SamplePack not available\n");
		currentTestDone = true;
		return;
	}
	StreamingBuffer* sb = &(samplePack->streamingBuffer);
	sb->processBlockwise();

	// Utility lambdas for configuring sweep parameters and picking samples.
	auto buildRange = [](int minVal, int maxVal, int step){
		std::vector<int> range;
		if(minVal > maxVal) std::swap(minVal, maxVal);
		if(step <= 0 || minVal == maxVal){
			range.push_back(minVal);
			if(range.back() != maxVal){
				range.push_back(maxVal);
			}
			return range;
		}
		for(int value = minVal; value <= maxVal; value += step){
			range.push_back(value);
		}
		if(range.empty() || range.back() != maxVal){
			range.push_back(maxVal);
		}
		return range;
	};

	auto getSampleSelection = [](const std::vector<std::pair<int,int>>* availableIds, int desiredCount){
		std::vector<std::pair<int,int>> selection;
		if(!availableIds || availableIds->empty() || desiredCount <= 0){
			return selection;
		}
		const auto& ids = *availableIds;
		const size_t availableSize = ids.size();
		selection.reserve(desiredCount);
		for(int i = 0; i < desiredCount; ++i){
			selection.push_back(ids[static_cast<size_t>(i) % availableSize]);
		}
		return selection;
	};

	// State machine that advances one scheduling step per audio block.
	auto runExperiment = [&, this](int minChunkMs, int maxChunkMs, int chunkStepMs,
		int minSamples, int maxSamples, int sampleStep, float timeDeltaSeconds,
		int runs) -> std::map<ConfigKey, ConfigResult>& {

		double now = this->blocksToSeconds(blocksElapsed);

		if(!state.initialized){
			state.resetState();
			state.initialized = true;
			state.timeDelta = std::max(0.0f, timeDeltaSeconds);
			state.runsTotal = std::max(1, runs);
			state.chunkSizesMs = buildRange(std::max(1, minChunkMs), std::max(1, maxChunkMs), chunkStepMs);
			state.sampleCounts = buildRange(std::max(1, minSamples), std::max(1, maxSamples), sampleStep);
			if(state.chunkSizesMs.empty()){
				state.chunkSizesMs.push_back(std::max(1, minChunkMs));
			}
			if(state.sampleCounts.empty()){
				state.sampleCounts.push_back(std::max(1, minSamples));
			}
			state.availableSampleIds = &sb->getAvailableSamples();
			state.availableStartCount = state.availableSampleIds ? state.availableSampleIds->size() : 0;
			state.phase = Phase::TriggerStart;
		}

		if(state.complete){
			state.phase = Phase::Complete;
			return state.results;
		}

		switch(state.phase){
			case Phase::TriggerStart: {
				if(sb->isStreaming()){
					break;
				}
				state.resetTransient();
				if(state.chunkIndex >= state.chunkSizesMs.size()){
					state.chunkIndex = 0;
				}
				if(state.sampleIndex >= state.sampleCounts.size()){
					state.sampleIndex = 0;
				}
				state.currentKey.chunkMs = state.chunkSizesMs[state.chunkIndex];
				state.currentKey.sampleCount = state.sampleCounts[state.sampleIndex];
				int chunkMs = std::max(1, state.currentKey.chunkMs);
				state.chunkSamples = static_cast<size_t>(std::max(1.0f,
						44100.0f * static_cast<float>(chunkMs) / 1000.0f));
				state.chunkBytes = state.chunkSamples * sizeof(float);
				state.targetStartJob = sb->getStartJobsIssued() + 1;
				sb->initForFolder(folderPath, state.chunkSamples);
				state.waitingForStartFlagRise = true;
				state.phaseStartTime = now;
				state.phase = Phase::AwaitStart;
				break;
			}
			case Phase::AwaitStart: {
				if(state.targetStartJob == 0){
					state.targetStartJob = sb->getStartJobsIssued() + 1;
				}
				if(state.waitingForStartFlagRise){
					if(sb->getStartJobsIssued() >= state.targetStartJob){
						state.waitingForStartFlagRise = false;
						state.phaseStartTime = now;
					}
					break;
				}
				if(sb->getStartJobsCompleted() < state.targetStartJob){
					break;
				}
				if(state.availableSampleIds){
					state.availableStartCount = state.availableSampleIds->size();
				}
				double elapsed = now - state.phaseStartTime;
				if(elapsed <= 0.0){
					elapsed = 1e-6;
				}
				float startBandwidth = 0.0f;
				double totalBytes = static_cast<double>(state.availableStartCount) * static_cast<double>(state.chunkBytes);
				if(totalBytes > 0.0){
					startBandwidth = static_cast<float>(totalBytes / (elapsed * 1024.0 * 1024.0));
				}
				state.results[state.currentKey].start.add(startBandwidth);
				state.cooldownEndTime = now + static_cast<double>(state.timeDelta);
				state.targetStartJob = 0;
				state.phase = (state.timeDelta > 0.0f) ? Phase::WaitBeforeChunk : Phase::TriggerChunks;
				break;
			}
			case Phase::WaitBeforeChunk: {
				if(now >= state.cooldownEndTime && !sb->isStreaming()){
					state.phase = Phase::TriggerChunks;
				}
				break;
			}
			case Phase::TriggerChunks: {
				if(sb->isStreaming()){
					break;
				}
				state.resetTransient();
				std::vector<std::pair<int,int>> selection = getSampleSelection(state.availableSampleIds, state.currentKey.sampleCount);
				if(selection.empty()){
					rt_printf("renderStreamingBandwidthTest: no samples available for chunk test\n");
					state.complete = true;
					state.phase = Phase::Complete;
					break;
				}
				for(const auto& identifier : selection){
					size_t requestId = sb->requestSample(identifier);
					int length = sb->getRequestSampleLength(requestId);
					if(length > 0){
						state.totalChunkSamples += static_cast<size_t>(length);
					}
				}
				state.targetChunkJob = sb->getChunkJobsIssued() + 1;
				state.waitingForChunkFlagRise = true;
				state.phaseStartTime = now;
				state.phase = Phase::AwaitChunks;
				break;
			}
			case Phase::AwaitChunks: {
				if(state.targetChunkJob == 0){
					state.targetChunkJob = sb->getChunkJobsIssued() + 1;
				}
				if(state.waitingForChunkFlagRise){
					if(sb->getChunkJobsIssued() >= state.targetChunkJob){
						state.waitingForChunkFlagRise = false;
						state.phaseStartTime = now;
					}
					break;
				}
				if(sb->getChunkJobsCompleted() < state.targetChunkJob){
					break;
				}
				double elapsed = now - state.phaseStartTime;
				if(elapsed <= 0.0){
					elapsed = 1e-6;
				}
				float chunkBandwidth = 0.0f;
				double totalBytes = static_cast<double>(state.totalChunkSamples) * sizeof(float);
				if(totalBytes > 0.0){
					chunkBandwidth = static_cast<float>(totalBytes / (elapsed * 1024.0 * 1024.0));
				}
				state.results[state.currentKey].chunk.add(chunkBandwidth);
				state.targetChunkJob = 0;
				state.sampleIndex++;
				if(state.sampleIndex >= state.sampleCounts.size()){
					state.sampleIndex = 0;
					state.chunkIndex++;
				}
				if(state.chunkIndex >= state.chunkSizesMs.size()){
					state.chunkIndex = 0;
					state.currentRun++;
				}
				if(state.currentRun >= state.runsTotal){
					state.complete = true;
					state.phase = Phase::Complete;
				} else {
					state.cooldownEndTime = now + static_cast<double>(state.timeDelta);
					state.phase = (state.timeDelta > 0.0f) ? Phase::WaitBeforeNextConfig : Phase::TriggerStart;
				}
				break;
			}
			case Phase::WaitBeforeNextConfig: {
				if(now >= state.cooldownEndTime && !sb->isStreaming()){
					state.phase = Phase::TriggerStart;
				}
				break;
			}
			case Phase::Complete:
			default:
				state.complete = true;
				break;
		}

		return state.results;
	};

	const int minChunkMs = 50;
	const int maxChunkMs = 300;
	const int chunkStepMs = 50;
	const int minSamples = 5;
	const int maxSamples = 25;
	const int sampleStep = 5;
	const float timeDeltaSeconds = 0.0f;
	const int runs = 3;

	auto& results = runExperiment(minChunkMs, maxChunkMs, chunkStepMs,
		minSamples, maxSamples, sampleStep, timeDeltaSeconds, runs);

	if(state.complete && !state.printed){
		for(auto& entry : results){
			entry.second.start.finalize();
			entry.second.chunk.finalize();
			rt_printf("Streaming BW | chunk %d ms, samples %d | starts: %.3f MB/s (std %.3f) | chunks: %.3f MB/s (std %.3f)\n",
				entry.first.chunkMs,
				entry.first.sampleCount,
				entry.second.start.mean,
				entry.second.start.stddev,
				entry.second.chunk.mean,
				entry.second.chunk.stddev);
		}
		state.printed = true;
		currentTestDone = true;
		blocksElapsed = 0;
		state.initialized = false;
	}

	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++){
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++){
			audioWrite(context, n, ch, 0.0f);
		}
	}
}
