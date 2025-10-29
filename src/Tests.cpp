#include <Bela.h>

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
	static int blocksElapsed = 0;
	blocksElapsed++;
	static int note = 0;

	if(secondsElapsed(blocksElapsed, 1)){
		rt_printf("starting playback of testSample on repeat\n");
		resourceManager->getVoices()->triggerVoice(resourceManager->getTestSample(), 0, 0, 0, 1, 0, 1, 1, true);
	}
	if(secondsElapsed(blocksElapsed, 2) ){
		rt_printf("stopping playback of testSample on repeat\n");
		resourceManager->getVoices()->triggerOff(0);
	}
	if(secondsElapsed(blocksElapsed, 3) ){
		rt_printf("starting playback of testSample with long adsr on repeat\n");
		resourceManager->getVoices()->triggerVoice(resourceManager->getTestSample(), 1, 0.5, 0.5, 0.5, 0.5, 1, 1, true);
	}
	if(secondsElapsed(blocksElapsed, 5) ){
		rt_printf("stopping playback of first testSample on repeat\n");
		resourceManager->getVoices()->triggerOff(1);
	}
	if(secondsElapsed(blocksElapsed, 6)){
		rt_printf("now creating a new voice every 0.1 s with increasing playbackrate\n");
	}
	if(blocksElapsed > 6 * resourceManager->blocksPerSecond){
		if(blocksElapsed % (int)(0.1 * resourceManager->blocksPerSecond) == 0){
			rt_printf("triggering note %d\n", note);
			note++;
			if(note > 32) note = 0;
			resourceManager->getVoices()->triggerVoice(resourceManager->getTestSample(), note, 0.01, 0.1, 0.7, 0.1, 0.5, 1, true);
		}
	}
	if(secondsElapsed(blocksElapsed, 9.2) ){
		rt_printf("should be saturated now\n");
	}
	
	
	
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = resourceManager->getVoices()->process();
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch, 0.1 * frame);
        }
	}
	if(secondsElapsed(blocksElapsed, 12) ){
		for(int i = 0; i <= 32; i++){
			resourceManager->getVoices()->triggerOff(i);
		}
		currentTestDone = true;
		blocksElapsed = 0;
	}
}

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
		blockFrames.at(n) = 0.0f;
		if(blocksElapsed < 3*44100/16) blockFrames.at(n) += testSample->at(testSampleIndex);
		for(unsigned int ch = 0; ch < context->audioInChannels; ch++) {
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

void ModeManager::renderMidiTest(BelaContext *context, ResourceManager* resourceManager){
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

void ModeManager::renderStreamingBandwidthTest(BelaContext *context, ResourceManager* resourceManager){
	static int blocksElapsed = 0;
	blocksElapsed++;
	static bool stopTest = false;
	//streaming to the big buffer of the samplePack
	static SamplePack* samplePack = resourceManager->getKeyInstrumentSamplePack();
	static StreamingBuffer* sb = &(samplePack->streamingBuffer);

	//static std::string folderPath = "/root/Samples/converted_new/";
	static std::string folderPath = "/mnt/sdcard/Samples/converted_new/";

	static std::vector<size_t> requestIds;
	static int note = 21;
	static int velocity = 1;
	static int maximumVelocity = 16;
	if(folderPath[1] == 'r'){
		maximumVelocity = 5;
	}
	static float nextPlayTime = 1;
	static float nextPrintTime = 1;
	static int milliseconds = 10;
	static int totalNumberOfSamples = 480;
	static int nrSamples = 5;
	static float timeDelta = 5.0f;
	static float timeEps = 0.1f;
	static float loadStartsTime = 1.0f;
	static float loadChunksTime = loadStartsTime + timeDelta;;
	static float lastTime = 0.0f;
	static bool oldStreamLoadingFlag = false;
	static bool chunksTime = true;

//#define VERSION1

#ifdef VERSION1
	//VERSION 1 (works)
	//Try how many files can be streamed before it fails. approx 23 @ 4MB to 1MB per sample.
	//the way it is now it will intentionally crash
	static int numberOfSamplesPlayed = 5;
	static float timeStep = 0.2;
	if(secondsElapsed(blocksElapsed, nextPlayTime)){
		//sb->printInfo();
		requestIds.push_back(sb->requestSample({note,velocity}));
		note += 3;
		velocity += 1;
		if(velocity>16) velocity = 1;
		
		nextPlayTime += timeStep; 
		if(nextPlayTime >  1 + numberOfSamplesPlayed * timeStep) nextPlayTime = 1.0;
	}
	if(secondsElapsed(blocksElapsed, 10 + timeStep * numberOfSamplesPlayed)){
		rt_printf("stop test trigger 1\n");
		stopTest = true;
	}
	sb->processBlockwise();


#else
	//VERSION 2
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
	static int totalSampleLength = 0;
	if(secondsElapsed(blocksElapsed, loadStartsTime)){
		loadStartsTime += 2 * timeDelta;
		sb->initForFolder(folderPath, 44100 * (float)milliseconds / 1000);
		lastTime = this->blocksToSeconds(blocksElapsed);
		rt_printf("-----------------init loading starts at %f----------------\n", lastTime);
	}	
	if(secondsElapsed(blocksElapsed, loadChunksTime)){
		loadChunksTime += 2 * timeDelta;
		lastTime = this->blocksToSeconds(blocksElapsed);
		rt_printf("-----------------init loading chunks at %f----------------\n", lastTime);
		for(int i = 0; i < nrSamples; i++){
			requestIds.push_back(sb->requestSample({note, velocity}));
			totalSampleLength += sb->getRequestSampleLength(requestIds.back());
			note += 3;
			velocity += 1;
			if(velocity>maximumVelocity) velocity = 1;
		}
	}

	sb->processBlockwise();

	bool newStreamloadingFlag = resourceManager->getStreamLoadingFlag();
	if(oldStreamLoadingFlag == true && newStreamloadingFlag == false){
		chunksTime = !chunksTime;
		float oldTime = lastTime;
		lastTime = this->blocksToSeconds(blocksElapsed);
		rt_printf("-----------------stopped last phase at at %f----------------\n", lastTime);
		float time = lastTime - oldTime;
		int chunkSize = (int)((float)milliseconds / 1000 * 44100 * 4);
		if(chunksTime){
			float BW = (float)(totalSampleLength * 4) / (time * 1024.0f * 1024.0f);
			rt_printf("time taken to load chunks: %f s at chunksize %d B means %f MB/s\n", time, chunkSize, BW);
		} else {
			float BW = (float)(totalNumberOfSamples * chunkSize) / (time * 1024.0f * 1024.0f);
			rt_printf("time taken to load starts: %f s at chunksize %d B means %f MB/s\n", time, chunkSize, BW);
		}
	}
	oldStreamLoadingFlag = newStreamloadingFlag;

	if(secondsElapsed(blocksElapsed, loadStartsTime - timeEps)){
		if(milliseconds < 350){
			rt_printf("increasing chunk size and restarting test\n");
			milliseconds += 50;
			note = 21;
			velocity = 1;
			totalSampleLength = 0;
		} else {
			rt_printf("stop test trigger 3: milliseconds: %d\n", milliseconds);
			stopTest = true;
		}
	}

#endif




	//playback (not necessary for the test)
	static bool startedPlay = false;
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = 0.0f;
		for(size_t requestId  = 1 ; requestId < nrSamples; requestId += 1){
			frame += sb->getNextSample(requestId) / nrSamples;
			if(!floatIsEqual(frame, 0.0f) && !startedPlay){
				startedPlay = true;
				rt_printf("STarted play at time: %f\n",this->blocksToSeconds(blocksElapsed));
			}
		}
		
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch,  frame);
        }
	}
		


	if(stopTest){
		stopTest = false;
		currentTestDone = true;
		nextPrintTime = 1;
		nextPlayTime = 1;
		note = 21;
		velocity = 1;
		sb->clearContainers(); // so that i can run the test multiple times for statistics.
		blocksElapsed = 0;
		milliseconds = 50;
	}
}
