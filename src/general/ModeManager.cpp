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
					"StreamingBandwidthTest",
					"CreateOsciSamples"
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
		case Mode::CreateOsciSamples:
			renderCreateOsciSamples(context, resourceManager);
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
}

void ModeManager::renderCreateOsciSamples(BelaContext *context, ResourceManager& resourceManager){
	static size_t blocksElapsed = 0;
	blocksElapsed++;
	static ResourceManager& rm = resourceManager;
	static int midiNote = 35; //Note E0
	static int midiVelocity = 64;
	static float frequency = 440.0f * powf(2.0f, (midiNote - 69.0f) / 12.0f);
	static float sampleLengthInSeconds = 1.0f/frequency;
	static int totalSamples = static_cast<int>(sampleLengthInSeconds * resourceManager.audioFramesPerSecond);
	static std::vector<float> sample(totalSamples, 0.0f);
	static bool recordingSine = false;
	static bool recordingSaw = false;
	static bool recordingSquare = false;
	static int recordingIndex = 0;

	rm.getRecorder().processBlockwise();

	if(blocksElapsed == 1){
		rt_printf("Creating Oscillator Sample for note %d at frequency %.2f Hz\n", midiNote, frequency);
		//TODO: remove this whole testSampleThing
		//The following will drop blocks - too lazy to make task do it
		for(int i = 0; i < totalSamples; i++){
			float time = (float)i / resourceManager.audioFramesPerSecond;
			float omega = 2 * M_PI * frequency;
			sample[i] = 0.1f * sinf(omega * time);
		}
	}
	if(blocksElapsed == resourceManager.blocksPerSecond * 2){ // wait 2 seconds for the initialization to finish
		recordingSine = true;
		std::string filePath = resourceManager.SAMPLES_PATH + "SineOscillator/" + std::to_string(midiNote) + "_" + std::to_string(midiVelocity) + ".wav";
		rm.getRecorder().setNewFilename(filePath);
		rt_printf("Writing sample to %s\n", filePath.c_str());
		rm.getRecorder().startRecording();
	}
	if(recordingSine){
		for(int i = 0; i < resourceManager.audioFramesPerBlock; i++){
			if(recordingIndex >= 100 * totalSamples){
				recordingSine = false;
				rm.getRecorder().stopRecording();
				rt_printf("Finished writing sample.\n");
				break;
			}
			float nextSample = sample[recordingIndex % totalSamples];
			if(recordingIndex < totalSamples && recordingIndex % (totalSamples / 8) == 0){
				int recordingSegment = recordingIndex / (totalSamples / 8);
				rt_printf("Recording sample at %f pi: %.5f * 0.1\n", (float)recordingSegment * 0.25f, nextSample * 10.0f);
			}
			rm.getRecorder().process(nextSample);
			recordingIndex++;
		}
	}
	
	if(blocksElapsed == resourceManager.blocksPerSecond * 3.5){
		rt_printf("Creating Saw Oscillator Sample for note %d at frequency %.2f Hz\n", midiNote, frequency);
		for(int i = 0; i < totalSamples; i++){
			float time = (float)i / resourceManager.audioFramesPerSecond;
			float omega = 2 * M_PI * frequency;
			sample[i] = 0.1f * (2.0f * (time * frequency - floorf(0.5f + time * frequency)));
		}
	}
	if(blocksElapsed == resourceManager.blocksPerSecond * 4){
		recordingIndex = 0;
		recordingSaw = true;
		std::string filePath = resourceManager.SAMPLES_PATH + "SawOscillator/" + std::to_string(midiNote) + "_" + std::to_string(midiVelocity) + ".wav";
		rm.getRecorder().setNewFilename(filePath);
		rt_printf("Writing sample to %s\n", filePath.c_str());
		rm.getRecorder().startRecording();
	}
	if(recordingSaw){
		for(int i = 0; i < resourceManager.audioFramesPerBlock; i++){
			if(recordingIndex >= 100 * totalSamples){
				recordingSaw = false;
				rm.getRecorder().stopRecording();
				rt_printf("Finished writing sample.\n");
				break;
			}
			float nextSample = sample[recordingIndex % totalSamples];
			if(recordingIndex < totalSamples && recordingIndex % (totalSamples / 8) == 0){
				int recordingSegment = recordingIndex / (totalSamples / 8);
				rt_printf("Recording sample at %f pi: %.5f * 0.1\n", (float)recordingSegment * 0.25f, nextSample * 10.0f);
			}
			rm.getRecorder().process(nextSample);
			recordingIndex++;
		}
	}
	if(blocksElapsed == resourceManager.blocksPerSecond * 5.5){
		rt_printf("Creating Square Oscillator Sample for note %d at frequency %.2f Hz\n", midiNote, frequency);
		for(int i = 0; i < totalSamples; i++){
			float time = (float)i / resourceManager.audioFramesPerSecond;
			float omega = 2 * M_PI * frequency;
			sample[i] = 0.1f * (sinf(omega * time) >= 0 ? 1.0f : -1.0f);
		}
	}
	if(blocksElapsed == resourceManager.blocksPerSecond * 6){
		recordingIndex = 0;
		recordingSquare = true;
		std::string filePath = resourceManager.SAMPLES_PATH + "SquareOscillator/" + std::to_string(midiNote) + "_" + std::to_string(midiVelocity) + ".wav";
		rm.getRecorder().setNewFilename(filePath);
		rt_printf("Writing sample to %s\n", filePath.c_str());
		rm.getRecorder().startRecording();
	}
	if(recordingSquare){
		for(int i = 0; i < resourceManager.audioFramesPerBlock; i++){
			if(recordingIndex >= 100 * totalSamples){
				recordingSquare = false;
				rm.getRecorder().stopRecording();
				rt_printf("Finished writing sample.\n");
				break;
			}
			float nextSample = sample[recordingIndex % totalSamples];
			if(recordingIndex < totalSamples && recordingIndex % (totalSamples / 8) == 0){
				int recordingSegment = recordingIndex / (totalSamples / 8);
				rt_printf("Recording sample at %f pi: %.5f * 0.1\n", (float)recordingSegment * 0.25f, nextSample * 10.0f);
			}
			rm.getRecorder().process(nextSample);
			recordingIndex++;
		}
	}
	
	if(blocksElapsed > resourceManager.blocksPerSecond * 10){ // wait 10 seconds max
		currentTestDone = true;
		blocksElapsed = 0;
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
