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
	testAll = false;
	currentTestDone = false;
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
						"ControllerTest",
						"SamplePackTest",
						"StreamingBandwidthTest"
					};
}

void ModeManager::render(BelaContext *context, ResourceManager* resourceManager){
	if(testAll){
		if(currentTestDone){
			currentTestDone = false;
			//TODO: use modeshift function here.
			int modeNumber = (int)mode;
			modeNumber++;
			if(modeNumber >= (int)Mode::COUNT){
				rt_printf("All Tests done. Back to top menu.\n");
				mode = Mode::TopMenu;
				testAll = false;
			} else {
				rt_printf("Running Test %s\n\n", modeNames.at(modeNumber).c_str());
				mode = (Mode)modeNumber;
			}
		}
	}
	if(currentTestDone){
		rt_printf("Single Test done. Back to top menu.\n");
		currentTestDone = false;
		mode = Mode::TopMenu;
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
		case Mode::SamplePack:
			renderSamplePackTest(context, resourceManager);
			break;
		case Mode::StreamingBandwidthTest:
			renderStreamingBandwidthTest(context, resourceManager);
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
				if(mode == Mode::AllTest){
					testAll = true;
					mode = Mode::BelaInterfaceTest;
					rt_printf("Running All Tests\n\n");
					rt_printf("Running Test %s\n\n", modeNames.at((int)mode).c_str());
				}
			}
		}
	}
}

void ModeManager::renderNormal(BelaContext *context, ResourceManager* resourceManager){
	for(unsigned int n = 0; n < resourceManager->audioFramesPerBlock; n++) {
		for(unsigned int ch = 0; ch < context->audioInChannels; ch++) {
            audioWrite(context, n, ch, 0.0f);
        }
	}
}


void ModeManager::modeShift(int indexShift, Mode* mode){
	if(indexShift != 1 && indexShift != -1) throw std::runtime_error("modeShift() used with indexshift unequal 1 or -1");
	int modeIndex = ((int)(*mode) + indexShift) % (int)Mode::COUNT;
	if(modeIndex < 0) modeIndex += (int)Mode::COUNT;
	if(modeIndex == (int)Mode::COUNT) modeIndex += indexShift;
	*mode = (Mode)(modeIndex);
}

bool ModeManager::secondsElapsed(int blocksElapsed, float seconds){
	return blocksElapsed  == (int)(seconds * resourceManager->blocksPerSecond);
}

float ModeManager::blocksToSeconds(int blocksElapsed){
	return (float)blocksElapsed / (float)resourceManager->blocksPerSecond;
}
