#include <Bela.h>
#include <cassert>
#include <cmath>
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
					"InstrumentSetEditor",
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
		case Mode::InstrumentSetEditor:
			renderInstrumentSetEditor(context, resourceManager);
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
	resourceManager.getUI().unlockStateNavigation();
	instrumentSetEditorPrimed = false;

	static size_t blocksElapsed = 0;
	blocksElapsed++;
	static bool initialPotSyncDone = false;
	
	assert(context != nullptr);
	IDisplayContext& display = resourceManager.getDisplayContext();
	BelaInterface* interface = resourceManager.getBelaInterface();
	DeviceMap& deviceMap = resourceManager.getDeviceMap();
	Mixer& mixer = resourceManager.getMixer();
	
	display.processBlockwise();
	assert(interface != nullptr);
	interface->processBlockwise();

	if(!initialPotSyncDone){
		for(const auto& pinToGain : deviceMap.pottiPinToGainId){
			int pin = pinToGain.first;
			GainId gainId = pinToGain.second;
			if(gainId >= GainId::COUNT) continue;
			float value = clamp(analogRead(context, 0, pin), 0.0f, 1.0f);
			if(deviceMap.reversePottis) value = 1.0f - value;
			mixer.setGain(gainId, value);
		}
		initialPotSyncDone = true;
		rt_printf("TopMenu: initial potentiometer scan applied\n");
	}

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
		if(msg.type == InterfaceMessageType::PotSignal){
			auto it = deviceMap.pottiPinToGainId.find(msg.id);
			if(it != deviceMap.pottiPinToGainId.end() && it->second < GainId::COUNT){
				mixer.setGain(it->second, msg.value);
			}
			continue;
		}
		if(msg.type == InterfaceMessageType::RotEncSignal){
			RotaryEncoderEvent event = msg.event;
			// UI-level direction mapping mirrors InputHandler rotary semantics.
			if(event == RotaryEncoderEvent::Right) modeShift(1,&selectionMode);
			if(event == RotaryEncoderEvent::Left) modeShift(-1,&selectionMode);
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
	resourceManager.getUI().unlockStateNavigation();
	instrumentSetEditorPrimed = false;
	resourceManager.processBlockwise();
}

void ModeManager::renderInstrumentSetEditor(BelaContext *context, ResourceManager& resourceManager){
	(void)context;
	resourceManager.keysInMelodicMode = true;
	if(!instrumentSetEditorPrimed){
		InstrumentSetLibrary& setLibrary = resourceManager.getInstrumentSetLibrary();
		InstrumentCatalog& catalog = resourceManager.getInstrumentCatalog();
		std::string folderToLoad = catalog.getFolderName(0);
		const int activeSetIndex = setLibrary.getActiveSetIndex();
		if(setLibrary.getEntryCount(activeSetIndex) > 0){
			folderToLoad = setLibrary.getEntry(activeSetIndex, 0).instrumentId;
		}
		resourceManager.getKeyInstrumentSamplePack().initForFolder(folderToLoad);
		instrumentSetEditorPrimed = true;
	}
	resourceManager.getUI().lockStateNavigation(UIStateId::SetEditor);
	resourceManager.processBlockwise();
}

void ModeManager::renderCreateOsciSamples(BelaContext *context, ResourceManager& resourceManager){
	static size_t blocksElapsed = 0;
	blocksElapsed++;
	static ResourceManager& rm = resourceManager;
	static int midiNote = 35; //Note E0
	static int midiVelocity = 64;
	static float frequency = 440.0f * powf(2.0f, (midiNote - 69.0f) / 12.0f);
	static int samplesPerPeriod = [] (float sampleRate, float freq){
		int periodSamples = static_cast<int>(std::round(sampleRate / freq));
		return periodSamples < 2 ? 2 : periodSamples;
	}(resourceManager.audioFramesPerSecond, frequency);
	static float effectiveFrequency = resourceManager.audioFramesPerSecond / static_cast<float>(samplesPerPeriod);
	static int totalSamples = samplesPerPeriod;
	static constexpr float targetSampleLengthSeconds = 20.0f;
	static std::vector<float> sample(totalSamples, 0.0f);
	static bool recordingSine = false;
	static bool recordingSaw = false;
	static bool recordingSquare = false;
	static bool preparedSine = false;
	static bool preparedSaw = false;
	static bool preparedSquare = false;
	static bool startedSine = false;
	static bool startedSaw = false;
	static bool startedSquare = false;
	static int recordingIndex = 0;
	const int targetFrames = std::max(1, static_cast<int>(std::round(targetSampleLengthSeconds * resourceManager.audioFramesPerSecond)));
	const int periodsToWrite = std::max(1, static_cast<int>(std::ceil(static_cast<float>(targetFrames) / static_cast<float>(totalSamples))));
	const int totalFramesToWrite = periodsToWrite * totalSamples;
	const float recordingDurationSeconds = static_cast<float>(totalFramesToWrite) / resourceManager.audioFramesPerSecond;
	const float stageGapSeconds = 0.5f;
	const float sineRecordStartSeconds = 2.0f;
	const float sawCreateSeconds = sineRecordStartSeconds + recordingDurationSeconds + stageGapSeconds;
	const float sawRecordStartSeconds = sawCreateSeconds + stageGapSeconds;
	const float squareCreateSeconds = sawRecordStartSeconds + recordingDurationSeconds + stageGapSeconds;
	const float squareRecordStartSeconds = squareCreateSeconds + stageGapSeconds;
	const float allDoneSeconds = squareRecordStartSeconds + recordingDurationSeconds + 1.0f;

	rm.getRecorder().processBlockwise();

	auto phaseAt = [](int sampleIndex, float phaseOffset) -> float {
		float phase = static_cast<float>(sampleIndex) / static_cast<float>(totalSamples);
		phase += phaseOffset;
		phase = phase - floorf(phase); // keep [0,1)
		return phase;
	};

	if(!preparedSine){
		rt_printf("Creating Oscillator Sample for note %d target=%.4fHz effective=%.4fHz periodSamples=%d\n",
			midiNote, frequency, effectiveFrequency, totalSamples);
		//TODO: remove this whole testSampleThing
		//The following will drop blocks - too lazy to make task do it
		for(int i = 0; i < totalSamples; i++){
			float phase = phaseAt(i, 0.0f);
			sample[i] = 0.1f * sinf(2.0f * M_PI * phase);
		}
		preparedSine = true;
	}
	if(!startedSine && blocksElapsed >= static_cast<size_t>(resourceManager.blocksPerSecond * sineRecordStartSeconds)){ // wait for initialization
		recordingIndex = 0;
		recordingSine = true;
		startedSine = true;
		std::string filePath = resourceManager.SAMPLES_PATH + "instruments/Oscillators/SineOscillator/" + std::to_string(midiNote) + "_" + std::to_string(midiVelocity) + ".wav";
		rm.getRecorder().setNewFilename(filePath);
		rt_printf("Writing sample to %s\n", filePath.c_str());
		rm.getRecorder().startRecording();
	}
		if(recordingSine){
			for(int i = 0; i < resourceManager.audioFramesPerBlock; i++){
				if(recordingIndex >= totalFramesToWrite){
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
	
	if(!preparedSaw && blocksElapsed >= static_cast<size_t>(resourceManager.blocksPerSecond * sawCreateSeconds)){
		rt_printf("Creating Saw Oscillator Sample for note %d at frequency %.2f Hz\n", midiNote, effectiveFrequency);
		for(int i = 0; i < totalSamples; i++){
			// Start at an upward zero crossing and keep the period exact in samples.
			float phase = phaseAt(i, 0.5f);
			sample[i] = 0.1f * (2.0f * phase - 1.0f);
		}
		preparedSaw = true;
	}
	if(!startedSaw && blocksElapsed >= static_cast<size_t>(resourceManager.blocksPerSecond * sawRecordStartSeconds)){
		recordingIndex = 0;
		recordingSaw = true;
		startedSaw = true;
		std::string filePath = resourceManager.SAMPLES_PATH + "instruments/Oscillators/SawOscillator/" + std::to_string(midiNote) + "_" + std::to_string(midiVelocity) + ".wav";
		rm.getRecorder().setNewFilename(filePath);
		rt_printf("Writing sample to %s\n", filePath.c_str());
		rm.getRecorder().startRecording();
	}
		if(recordingSaw){
			for(int i = 0; i < resourceManager.audioFramesPerBlock; i++){
				if(recordingIndex >= totalFramesToWrite){
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
	if(!preparedSquare && blocksElapsed >= static_cast<size_t>(resourceManager.blocksPerSecond * squareCreateSeconds)){
		rt_printf("Creating Square Oscillator Sample for note %d at frequency %.2f Hz\n", midiNote, effectiveFrequency);
		for(int i = 0; i < totalSamples; i++){
			// Square-like from odd harmonics: periodic in exactly totalSamples and
			// crossing zero at loop start with positive slope.
			float phase = phaseAt(i, 0.0f);
			float value = 0.0f;
			const int maxOddHarmonic = 31;
			for(int harmonic = 1; harmonic <= maxOddHarmonic; harmonic += 2){
				value += sinf(2.0f * M_PI * static_cast<float>(harmonic) * phase) / static_cast<float>(harmonic);
			}
			value *= (4.0f / M_PI);
			sample[i] = 0.1f * clamp(value, -1.0f, 1.0f);
		}
		preparedSquare = true;
	}
	if(!startedSquare && blocksElapsed >= static_cast<size_t>(resourceManager.blocksPerSecond * squareRecordStartSeconds)){
		recordingIndex = 0;
		recordingSquare = true;
		startedSquare = true;
		std::string filePath = resourceManager.SAMPLES_PATH + "instruments/Oscillators/SquareOscillator/" + std::to_string(midiNote) + "_" + std::to_string(midiVelocity) + ".wav";
		rm.getRecorder().setNewFilename(filePath);
		rt_printf("Writing sample to %s\n", filePath.c_str());
		rm.getRecorder().startRecording();
	}
		if(recordingSquare){
			for(int i = 0; i < resourceManager.audioFramesPerBlock; i++){
				if(recordingIndex >= totalFramesToWrite){
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
	
	if(blocksElapsed > static_cast<size_t>(resourceManager.blocksPerSecond * allDoneSeconds)){
		currentTestDone = true;
		blocksElapsed = 0;
		recordingIndex = 0;
		recordingSine = false;
		recordingSaw = false;
		recordingSquare = false;
		preparedSine = false;
		preparedSaw = false;
		preparedSquare = false;
		startedSine = false;
		startedSaw = false;
		startedSquare = false;
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
