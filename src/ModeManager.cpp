#include <Bela.h>

#include "../include/ModeManager.h"
#include "../include/ResourceManager.h"
#include "../include/IDisplayContext.h"
#include "../include/BelaInterface.h"
#include "../include/Controller.h"
#include "../include/BasicUtilities.h"
#include "../include/Loopers.h"
#include "../include/Samplers.h"
#include "../include/Voices.h"

ModeManager::ModeManager(ResourceManager* resourceManager): resourceManager(resourceManager) {
	context = resourceManager->getBelaContext();
	mode = Mode::TopMenu;
	modeNames  = {
						"TopMenu",
						"Normal",
						"AllTest",
						"BelaInterfaceTest",
						"DisplayContextTest",
						"VoicesTest",
						"LoopersTest",
						"SamplersTest",
						"MidiTest",
						"ControllerTest"
					};
}

void ModeManager::render(BelaContext *context, ResourceManager* resourceManager){
	if(testAll){
		if(resourceManager->currentTestDone){
			resourceManager->currentTestDone = false;
			//TODO: use modeshift function here.
			int modeNumber = (int)mode;
			modeNumber++;
			if(modeNumber >= (int)Mode::COUNT){
				rt_printf("All Tests done. proceeding with normal render.\n");
				mode = Mode::Normal;
				testAll = false;
			} else {
				rt_printf("Running Test %s", modeNames.at(modeNumber).c_str());
				mode = (Mode)modeNumber;
			}
		}
	}
	if(resourceManager->currentTestDone){
		rt_printf("Single Test done. proceeding with normal render.\n");
		resourceManager->currentTestDone = false;
		mode = Mode::Normal;
	}
	switch(mode) {
		case Mode::TopMenu:
			renderTopMenu(context, resourceManager);
			break;
	    case Mode::Normal:
	        renderNormal(context, resourceManager);
	        break;
	    case Mode::AllTest:
	        rt_printf("ERROR: AllTest mode should not reach ModeManager::render()\n");
	        break;
	    case Mode::BelaInterfaceTest:
	        renderBelaInterfaceTest(context, resourceManager);
	        break;
		case Mode::DisplayContextTest:
	        renderDisplayContextTest(context, resourceManager);
	        break;
	    case Mode::VoicesTest:
	        renderVoicesTest(context, resourceManager);
	        break;
	    case Mode::LoopersTest:
	        renderLoopersTest(context, resourceManager);
	        break;
		case Mode::SamplersTest:
	        renderSamplersTest(context, resourceManager);
	        break;
	    case Mode::MidiTest:
	        renderMidiTest(context, resourceManager);
	        break;
	    case Mode::ControllerTest:
	        renderControllerTest(context, resourceManager);
	        break;
	    case Mode::COUNT:
	        rt_printf("ERROR: COUNT mode should not reach ModeManager::render()\n");
	        break;
	}
}

void ModeManager::renderTopMenu(BelaContext* context, ResourceManager* ResourceManager){
	IDisplayContext* display = resourceManager->getDisplayContext();
	BelaInterface* interface = resourceManager->getBelaInterface();
	
	display->processBlockwise();
	interface->processBlockwise();
	
	static Mode selectionMode = Mode::TopMenu;
	std::string modeName = modeNames.at((int)selectionMode);
	display->setLines(0, 0, "Run Mode", modeName.c_str(), "push to ", "continue");
	
	while(interface->numAvailableMessages() > 0){
		InterfaceMessage msg = interface->getNextInterfaceMessage();
#ifdef DEBUG_BUILD
		msg.prettyPrint();
#endif
		if(msg.type == InterfaceMessageType::RotEncSignal){
			RotaryEncoderEvent event = msg.event;
			if(event == RotaryEncoderEvent::Left) modeShift(-1,&selectionMode);
			if(event == RotaryEncoderEvent::Right) modeShift(1,&selectionMode);
			if(event == RotaryEncoderEvent::Push){
				mode = selectionMode;
				if(mode == Mode::AllTest) testAll = true;
				ResourceManager->currentTestDone = true;
			}
		}
	}
}

void ModeManager::renderNormal(BelaContext *context, ResourceManager* resourceManager){}

void ModeManager::renderBelaInterfaceTest(BelaContext *context, ResourceManager* resourceManager){
	static int blocksElapsed = 0;
	blocksElapsed++;

	resourceManager->getDisplayContext()->processBlockwise();

	BelaInterface* interface = resourceManager->getBelaInterface();
	interface->processBlockwise();
	while(interface->numAvailableMessages() > 0){
		InterfaceMessage msg = interface->getNextInterfaceMessage();
		msg.prettyPrint();
		if(msg.type == InterfaceMessageType::ButtonPressed && msg.id == 0) resourceManager->currentTestDone = true;
	}
	
	if(blocksElapsed%(2 * resourceManager->audioFramesPerSecond / resourceManager->audioFramesPerBlock) == 0){
		interface->printPotentiometerValues();
	}
	if(secondsElapsed(blocksElapsed, 10) ){
		resourceManager->currentTestDone = true;
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
		//TODO: i got a segfault somewhere around here and then it suddenly went away without me doing anything???
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
		resourceManager->currentTestDone = true;
	}
}

void ModeManager::renderVoicesTest(BelaContext *context, ResourceManager* resourceManager){
	static int blocksElapsed = 0;
	blocksElapsed++;

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
	if(secondsElapsed(blocksElapsed, 9) ){
		rt_printf("should be saturated now\n");
	}
	static int note = 0;
	if(blocksElapsed > 6 * resourceManager->audioFramesPerSecond / resourceManager->audioFramesPerBlock && blocksElapsed % (int)(0.1 * resourceManager->audioFramesPerSecond / resourceManager->audioFramesPerBlock) == 0){
		note++;
		if(note > 32) note = 0;
		resourceManager->getVoices()->triggerVoice(resourceManager->getTestSample(), note, 0.01, 0.1, 0.7, 0.1, 0.1, (float)note / 30, true);
	}
	
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		float frame = resourceManager->getVoices()->process();
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch, 0.1 * frame);
        }
	}
	if(secondsElapsed(blocksElapsed, 10) ){
		resourceManager->currentTestDone = true;
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
		msg.prettyPrint();
		if(msg.type == InterfaceMessageType::PotSignal && msg.id == 0) outputGain = msg.value;
		if(msg.type == InterfaceMessageType::PotSignal && msg.id == 1) inputGain = msg.value * 10.0;
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
		loopers->toggleRecord(0);
	}

	if(secondsElapsed(blocksElapsed, 5)){
		loopers->togglePlay(0);
	}
	loopers->processBlockwise(blockFrames);
	
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		for(unsigned int ch = 0; ch < context->audioInChannels; ch++) {
            audioWrite(context, n, ch, outputGain * blockFrames.at(n));
        }
	}
	if(secondsElapsed(blocksElapsed, 10) ){
		resourceManager->currentTestDone = true;
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
	if(secondsElapsed(blocksElapsed, 10) ){
		resourceManager->currentTestDone = true;
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
		resourceManager->currentTestDone = true;
	}
}

void ModeManager::renderControllerTest(BelaContext *context, ResourceManager* resourceManager){
	static int blocksElapsed = 0;
	blocksElapsed++;

	Controller* controller = resourceManager->getController();
	
	controller->processBlockwise();
	
	if((blocksElapsed * resourceManager->audioFramesPerSecond / resourceManager->audioFramesPerBlock)%2 == 0){
		//TODO: do something
	}
	if(secondsElapsed(blocksElapsed, 10) ){
		resourceManager->currentTestDone = true;
	}

}

void ModeManager::modeShift(int indexShift, Mode* mode){
	if(indexShift != 1 && indexShift != -1) throw std::runtime_error("modeShift() used with indexshift unequal 1 or -1");
	int modeIndex = ((int)(*mode) + indexShift) % (int)Mode::COUNT;
	if(modeIndex < 0) modeIndex += (int)Mode::COUNT;
	if(modeIndex == (int)Mode::COUNT) modeIndex += indexShift;
	*mode = (Mode)(modeIndex);
}

bool ModeManager::secondsElapsed(int blocksElapsed, int seconds){
	return blocksElapsed  == seconds * resourceManager->audioFramesPerSecond / resourceManager->audioFramesPerBlock ;
}
