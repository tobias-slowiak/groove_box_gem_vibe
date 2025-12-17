#include <Bela.h>

#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>
#include<unordered_map>
//compile

#include "../include/ModeManager.h"
#include "../include/ResourceManager.h"
#include "../include/StreamingBuffer.h"
#include "../include/IDisplayContext.h"
#include "../include/BelaInterface.h"
#include "../include/IMidi.h"
#include "../include/MidiReal.h"
#include "../include/MidiFake.h"
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
	static int note = 33;
	static int velocity = 1;
	SamplePack* sp = resourceManager->getKeyInstrumentSamplePack();
	Voices* voices = resourceManager->getVoices();

	//delayed start of playback bc otherwise it will stream too slow. investigate!

	if(blocksElapsed == 1){
		sp->triggerVoice(96,2);
		sp->triggerVoice(96,3);
		sp->triggerVoice(96,4);
		sp->triggerVoice(99,5);
		sp->triggerVoice(99,6);
	}

	if(blocksElapsed == 15000){
		sp->initForFolder("/mnt/sdcard/Samples/Standard_Bass");
	}
	static int sampleNumber = 0;
	static int blockOffset = 0;
	if(blocksElapsed == 20000){
		sp->triggerVoice(93,1);
	}
	if(blocksElapsed == 30000 + blockOffset){
		sampleNumber += 1;
		blockOffset += 500;
		note += 3;
		DEBUG_RT_PRINTF("\n\n\n sample %d \n\n\n", sampleNumber);
		sp->triggerVoice(note,10);
		if(sampleNumber == 10){
			currentTestDone = true;
			blocksElapsed = 0;
			for(int i = 33; i <= note; i++){
				sp->triggerOff(i);
			}
		}
	}

	sp->processBlockwise();
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = 0.0f;
		frame += voices->process() * 0.1;
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch,  frame);
        }
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
	assert(keyMidi != nullptr);
	IMidi* controlMidi = resourceManager->getControlMidi();
	assert(controlMidi != nullptr);
	
	IMidiParser* parser = keyMidi->getParser();
	assert(parser != nullptr);
	
	/*Sending messages in MidiFake Case*/
	if(!resourceManager->keyMidiConnected){
		if(secondsElapsed(blocksElapsed, 1)){
			parser->pushMessage(33, 64, 0, kmmNoteOn);
		}
	}
	if(!resourceManager->controlMidiConnected){
		if(secondsElapsed(blocksElapsed, 2)){
			controlMidi->getParser()->pushMessage(10, 64, 9, kmmNoteOn);
		}
	}
	/*MidiFake case done*/


	while(parser->numAvailableMessages() > 0) { //this line makes segfault if ...
        IMidiChannelMessage* message = parser->getNextChannelMessage();
		assert(message != nullptr);
        message->prettyPrint();
		//compile
	}
		
	
	parser = controlMidi->getParser();
	assert(parser != nullptr);
	while(parser->numAvailableMessages() > 0){
		IMidiChannelMessage* message = parser->getNextChannelMessage();
		assert(message != nullptr);
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

	/*Sending messages in MidiFake Case*/
	IMidi* keyMidi = resourceManager->getKeyMidi();
	if(!resourceManager->keyMidiConnected){
		if(secondsElapsed(blocksElapsed, 1) || secondsElapsed(blocksElapsed, 2)){
			keyMidi->getParser()->pushMessage(33, 1, 0, kmmNoteOn);
		}
	}

	/*MidiFake case done*/

	Controller* controller = resourceManager->getController();
	SamplePack* samplePack = resourceManager->getKeyInstrumentSamplePack();
	controller->processBlockwise();
	samplePack->processBlockwise();
	
	if((blocksElapsed * resourceManager->blocksPerSecond)%2 == 0){
		//TODO: do something
	}
	if(secondsElapsed(blocksElapsed, 20) ){
		currentTestDone = true;
		blocksElapsed = 0;
	}
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = 0.0f;
		float inFrame = 0.0f;
		frame += controller->process(inFrame);
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch,  frame);
        }
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
	Findings from old streamingbuffer class, should be comparable now, because the 
	bandwidth to the SD card is the limit and that did not chnange:
	the chunksize strongly influences the bandwidth. up to 200 ms the increase is
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
	static int blocksElapsed = 0;
	blocksElapsed++;

	SamplePack* samplePack = resourceManager->getKeyInstrumentSamplePack();
	Voices* voices = resourceManager->getVoices();
	static std::unordered_map<SampleIdentifier, size_t>& availableSamples = samplePack->getAvailableSamples();
	static size_t totalsizeinbytes;
	if(blocksElapsed == 1){
		for(auto& pairthing: availableSamples){
			totalsizeinbytes += pairthing.second * sizeof(float);
		}
		DEBUG_RT_PRINTF("total isze in bytes = %zu\n", totalsizeinbytes);
	}

	if(blocksElapsed == 1000){
		int i = 0;
		for(auto& sidlengthpair: availableSamples){
			i++;
			samplePack->streamFullSample(sidlengthpair.first);
		}
	}
	static bool streamerStarted = false;
	static int startBlock = 0;
	static int endBlock = 0;
	static bool done = false;
	if(blocksElapsed > 1001 && !done){
		if(!samplePack->streamerIsInFlight() && !streamerStarted){
			DEBUG_RT_PRINTF("waiting for streamer to start\n");
		}
		if(samplePack->streamerIsInFlight() && !streamerStarted){
			streamerStarted = true;
			startBlock = blocksElapsed;
			DEBUG_RT_PRINTF("start stream at block %d", startBlock);
		}
		if(!samplePack->streamerIsInFlight() && streamerStarted){
			endBlock = blocksElapsed;
			DEBUG_RT_PRINTF("done streaming at block %d\n", endBlock);
			int totalblocks = endBlock - startBlock;
			DEBUG_RT_PRINTF("so we took %d blocks\n", totalblocks);
			float totaltime = totalblocks * (float)resourceManager->audioFramesPerBlock / (float)resourceManager->audioFramesPerSecond;
			DEBUG_RT_PRINTF("which is a time of %f seconds\n", totalblocks);
			DEBUG_RT_PRINTF("which gives a bandwidth of %f MB/s\n", 1e-6 * totalsizeinbytes / totaltime);
			done = true;
		}
	}

	samplePack->processBlockwise();
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = 0.0f;
		//frame += voices->process();
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch,  frame);
        }
	}
}
