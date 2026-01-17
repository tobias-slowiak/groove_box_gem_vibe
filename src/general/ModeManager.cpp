#include <Bela.h>
#include <cassert>
//compiel
#include "../../include/general/ModeManager.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/hardwareInterfaces/IDisplayContext.h"
#include "../../include/hardwareInterfaces/BelaInterface.h"
#include "../../include/general/BasicUtilities.h"
#include "../../include/audio/Loopers.h"
#include "../../include/audio/Samplers.h"
#include "../../include/audio/Voices.h"

ModeManager::ModeManager(ResourceManager& resourceManager): resourceManager(resourceManager) {
	context = resourceManager.getBelaContext();
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

void ModeManager::render(BelaContext *context, ResourceManager& resourceManager){
	assert(context != nullptr);
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
				assert(modeNumber >= 0 && modeNames.size() > static_cast<size_t>(modeNumber));
				rt_printf("Running Test %s\n\n", modeNames.at(modeNumber).c_str());
				mode = (Mode)modeNumber;
			}
		}
	}
	if(currentTestDone){
		rt_printf("Single Test done. Back to top menu.\n");
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

void ModeManager::renderTopMenu(BelaContext* context, ResourceManager& resourceManager){

	static size_t blocksElapsed = 0;
	blocksElapsed++;
	
	assert(context != nullptr);
	IDisplayContext& display = resourceManager.getDisplayContext();
	BelaInterface* interface = resourceManager.getBelaInterface();
	
	display.processBlockwise();
	assert(interface != nullptr);
	interface->processBlockwise();

	static Mode selectionMode = Mode::TopMenu;
	
	if(blocksElapsed == 1){
		display.setLines({{"Run Mode", modeNames.at(static_cast<int>(selectionMode)).c_str(), "push to ", "continue"},{"modemngr"}});
	}
	if(currentTestDone){
		display.setLines({{"Run Mode", modeNames.at(static_cast<int>(selectionMode)).c_str(), "push to ", "continue"},{"modemngr"}});

		currentTestDone = false;
	}
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
				printf("set mode to %d\n", static_cast<int>(mode));
				
				if(mode == Mode::AllTest){
					testAll = true;
					mode = Mode::BelaInterfaceTest;
					rt_printf("Running All Tests\n\n");
					const int modeIndex = static_cast<int>(mode);
					assert(modeIndex >= 0 && modeNames.size() > static_cast<size_t>(modeIndex));
					rt_printf("Running Test %s\n\n", modeNames.at(modeIndex).c_str());
				}
			}
			display.setLines({{"Run Mode", modeNames.at(static_cast<int>(selectionMode)).c_str(), "push to ", "continue"},{"modemngr"}});
		}
	}

		for(unsigned int n = 0; n < resourceManager.audioFramesPerBlock; n++) {
		float frame = 0.0f;
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch,  frame);
        }
	}
}

void ModeManager::renderNormal(BelaContext *context, ResourceManager& resourceManager){
	resourceManager.processBlockwise();
	
	for(unsigned int n = 0; n < resourceManager.audioFramesPerBlock; n++) {
		float frame = resourceManager.getNextFrame(n);
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch,  frame);
        }
	}
}


void ModeManager::modeShift(int indexShift, Mode* mode){
	assert(mode != nullptr);
	if(indexShift != 1 && indexShift != -1) throw std::runtime_error("modeShift() used with indexshift unequal 1 or -1");
	int modeIndex = ((int)(*mode) + indexShift) % (int)Mode::COUNT;
	if(modeIndex < 0) modeIndex += (int)Mode::COUNT;
	if(modeIndex == (int)Mode::COUNT) modeIndex += indexShift;
	*mode = (Mode)(modeIndex);
}

bool ModeManager::secondsElapsed(int blocksElapsed, float seconds){
	return blocksElapsed  == (int)(seconds * resourceManager.blocksPerSecond);
}

float ModeManager::blocksToSeconds(int blocksElapsed){
	return (float)blocksElapsed / (float)resourceManager.blocksPerSecond;
}
