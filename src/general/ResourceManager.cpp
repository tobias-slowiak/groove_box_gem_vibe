#include <vector>
#include <algorithm>
#include <fcntl.h> 
#include <sys/ioctl.h>      
#include <linux/i2c-dev.h> 
#include <unistd.h> 
#include <cstdio>  
#include <cerrno>  
#include <stdio.h>
#include <cassert>
#include "../../include/general/ResourceManager.h"
#include "../../include/general/ModeManager.h"
#include "../../include/hardwareInterfaces/DeviceMap.h"
#include "../../include/hardwareInterfaces/BelaInterface.h"
#include "../../include/ui/UI.h"
#include "../../include/audio/Samplers.h"
#include "../../include/audio/Loopers.h"
#include "../../include/audio/SignalRouter.h"
#include "../../include/hardwareInterfaces/IDisplayContext.h"
#include "../../include/hardwareInterfaces/DisplayContextReal.h"
#include "../../include/hardwareInterfaces/DisplayContextFake.h"
#include "../../include/hardwareInterfaces/IMidi.h"
#include "../../include/hardwareInterfaces/MidiReal.h"
#include "../../include/hardwareInterfaces/MidiFake.h"
#include "../../include/audio/SamplePack.h"
#include "../../include/audio/InstrumentCatalog.h"

namespace {
SamplePackVoiceSettings toVoiceSettings(const InstrumentDefaults& defaults){
	SamplePackVoiceSettings out;
	out.attack = defaults.voice.attack;
	out.decay = defaults.voice.decay;
	out.sustain = defaults.voice.sustain;
	out.release = defaults.voice.release;
	out.repeat = defaults.voice.repeat;
	return out;
}

void applyEffectDefaultsToChain(EffectsChain& chain, const std::vector<EffectStageDefaults>& defaults){
	chain.clearEffects();
	for(const auto& effect : defaults){
		chain.addEffect(effect.type, effect.params);
	}
}
}


bool i2cDevicePresent(int bus, int address)
{
    char filename[20];
    snprintf(filename, 19, "/dev/i2c-%d", bus);

    int file = open(filename, O_RDWR);
    if (file < 0){
        return false;
    }

    if (ioctl(file, I2C_SLAVE, address) < 0) {
        close(file);
        return false;
    }

    // Try to write zero bytes (no data, just check ACK)
    unsigned char buf;
    ssize_t result = read(file, &buf, 1);
    close(file);
    return (result >=0);
}


void ResourceManager::setup(BelaContext* context){
	assert(context != nullptr);
	printf("------------------------------------\n");
	printf("-----ResourceManager setup----------\n");
	this->context = context;
	// modeManager will be created later after other dependencies
	
	//External devices 
	deviceMap.reset(new DeviceMap(*this));

	//Interface
	interfaceConnected = i2cDevicePresent(1, 0x3c);

	if(interfaceConnected){
		printf("Interface Connected\n");
		interface = new BelaInterface(*this);
		std::vector<U8G2*> u8g2s = initU8G2s(NUM_LINES_PER_DISPLAY);
	    displayContext.reset(new DisplayContextReal(*this, u8g2s, NUM_LINES_PER_DISPLAY));
	    displayContext->initDisplayContext();
		if(context->analogFrames == 0 || context->analogFrames > context->audioFrames) {
			rt_printf("Error: this example needs analog enabled, with 4 or 8 channels\n");
			throw std::runtime_error("Analog not enabled");
		}
	} else {
		printf("Interface not Connected\n");
		displayContext.reset(new DisplayContextFake());
	}

    //Midi
    std::unique_ptr<IMidi> tempKeyMidi(new MidiReal());
	keyMidiConnected = (tempKeyMidi->readFrom(deviceMap->keyMidiName.c_str()) == 1);
	if(keyMidiConnected){
		printf("Key Midi Device available.\n");
		tempKeyMidi->writeTo(deviceMap->keyMidiName.c_str());
		tempKeyMidi->enableParser(true);
		keyMidi = std::move(tempKeyMidi);
	} else {
		printf("Key Midi Device not available\n");
		keyMidi.reset(new MidiFake());
	}

	std::unique_ptr<IMidi> tempControlMidi(new MidiReal());
	controlMidiConnected = (tempControlMidi->readFrom(deviceMap->controlMidiName.c_str()) == 1);
	if(controlMidiConnected){
		printf("Control Midi Device available.\n");
		tempControlMidi->writeTo(deviceMap->controlMidiName.c_str());
    	tempControlMidi->enableParser(true);
		controlMidi = std::move(tempControlMidi);
	} else {
		printf("Control Midi Device not available\n");
		controlMidi.reset(new MidiFake());
	}
	#ifdef LAUNCHKEY_46_MK1
		controlMidi->writeMessage(0x90, 15, 12, 127); //control mode on to set looper light
	#endif
	#ifdef LAUNCHKEY_37_MK3
		controlMidi->writeMessage(0x90, 15, 12, 127); //control mode on to set looper light
	#endif
	//Constants
	audioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
	audioFramesPerSecond = context->audioSampleRate;
	audioInputChannels = context->audioInChannels;
	audioFramesPerBlock = context->audioFrames;
	blocksPerSecond = audioFramesPerSecond / audioFramesPerBlock;
	
	//Classes
	frames.resize((int)GainId::COUNT);
	mixer.reset(new Mixer(*this));
	printf("Constructed Mixer\n");
	keyInstrumentSamplePack.reset(new SamplePack(*this, "keyInstrument", instrumentCatalog.getFolderName(0), 44100 * 6 * 35)); // 6s approx 1MB. 35MB enables approx 32 simul samples
	keyInstrumentSamplePack->setVoiceSettings(toVoiceSettings(instrumentCatalog.getDefaults(0)));
	printf("Constructed keyInstrumentSamplePack\n");
	drumSamplePack.reset(new SamplePack(*this, "drum", drumCatalog.getFolderName(0), 44100 * 6 * 35)); // 6s approx 1MB. 35MB enables approx 32 simul samples
	drumSamplePack->setVoiceSettings(toVoiceSettings(drumCatalog.getDefaults(0)));
	printf("Constructed drumSamplePack\n");
	metronome.reset(new Metronome(*this));
	printf("Constructed Metronome\n");
	metronome->printBufferInfo();
	samplers.reset(new Samplers(*this));
	printf("Constructed Samplers\n");
	looperLights.reset(new LooperLights(*this));
	printf("Constructed LooperLights\n");
	looperLights->initialize();
	loopers.reset(new Loopers(*this));
	printf("Constructed Loopers\n");
	signalRouter.reset(new SignalRouter(*this));
	if(keysInMelodicMode){
		applyEffectDefaultsToChain(signalRouter->getInstrumentEffects(), instrumentCatalog.getDefaults(0).effects);
	} else {
		applyEffectDefaultsToChain(signalRouter->getInstrumentEffects(), drumCatalog.getDefaults(0).effects);
	}
	printf("Constructed SignalRouter\n");
	recorder.reset(new Recorder(*this, "/root/Bela/Samples/Recordings/recording.wav"));
	printf("Constructed Recorder\n");

	modeManager.reset(new ModeManager(*this));
	ui.reset(new UI(*this));
	printf("Constructed UI\n");
	inputHandler.reset(new InputHandler(*this));
	printf("Constructed InputHandler\n");
	

	this->makeTestSample();
	printf("Made TestSample\n");
	printf("------------------------------------\n");
}

void ResourceManager::cleanup(BelaContext* context){
	// unique_ptr handles automatic cleanup, no manual deletion needed
	if(interfaceConnected) delete interface;
	printf("cleanup done\n");
}

//#########GETTERS
BelaContext* ResourceManager::getBelaContext(){
	if (context)
		return context;
	else {
		rt_printf("ERROR: trying to getBelaContext(), but that is nullptr\n");
		throw std::runtime_error("getBelaContext() failed: is nullptr");
	}
}

ModeManager& ResourceManager::getModeManager(){
	if (modeManager)
		return *modeManager;
	else {
		rt_printf("ERROR: trying to getModeManager(), but that is nullptr\n");
		throw std::runtime_error("getModeManager() failed: is nullptr");
	}
}

DeviceMap& ResourceManager::getDeviceMap(){
	if (deviceMap)
		return *deviceMap;
	else {
		rt_printf("ERROR: trying to getDeviceMap(), but that is nullptr\n");
		throw std::runtime_error("getDeviceMap() failed: is nullptr");
	}
}

IDisplayContext& ResourceManager::getDisplayContext(){
	if (displayContext)
		return *displayContext;
	else {
		rt_printf("ERROR: trying to getDisplayContext(), but that is nullptr\n");
		throw std::runtime_error("getDisplayContext() failed: is nullptr");
	}
}

Metronome& ResourceManager::getMetronome(){
	if (metronome)
		return *metronome;
	else {
		rt_printf("ERROR: trying to getMetronome(), but that is nullptr\n");
		throw std::runtime_error("getMetronome() failed: is nullptr");
	}
}

Samplers& ResourceManager::getSamplers() {
	if (samplers)
		return *samplers;
	else {
		rt_printf("ERROR: trying to getSamplers(), but that is nullptr\n");
		throw std::runtime_error("getSamplers() failed: is nullptr");
	}
}

Loopers& ResourceManager::getLoopers(){
	if (loopers)
		return *loopers;
	else {
		rt_printf("ERROR: trying togetLoopers(), but that is nullptr\n");
		throw std::runtime_error("getLoopers() failed: is nullptr");
	}
}

SignalRouter& ResourceManager::getSignalRouter(){
	if (signalRouter)
		return *signalRouter;
	else {
		rt_printf("ERROR: trying to getSignalRouter(), but that is nullptr\n");
		throw std::runtime_error("getSignalRouter() failed: is nullptr");
	}
}

Recorder& ResourceManager::getRecorder(){
	if (recorder)
		return *recorder;
	else {
		rt_printf("ERROR: trying to getRecorder(), but that is nullptr\n");
		throw std::runtime_error("getRecorder() failed: is nullptr");
	}
}

InstrumentCatalog& ResourceManager::getInstrumentCatalog(){
	return instrumentCatalog;
}
DrumCatalog& ResourceManager::getDrumCatalog(){
	return drumCatalog;
}
Mixer& ResourceManager::getMixer(){
	return *mixer;
}

UI& ResourceManager::getUI(){
	return *ui;
}

BelaInterface* ResourceManager::getBelaInterface(){
	assert(interface != nullptr);
	return interface;
}

IMidi& ResourceManager::getKeyMidi(){
	if (this->keyMidi)
		return *this->keyMidi;
	else {
		rt_printf("ERROR: trying getKeyMidi(), but that is nullptr\n");
		throw std::runtime_error("getKeyMidi() failed: is nullptr");
	}
}

IMidi& ResourceManager::getControlMidi(){
	if (this->controlMidi)
		return *this->controlMidi;
	else {
		rt_printf("ERROR: trying getControlMidi(), but that is nullptr\n");
		throw std::runtime_error("getControlMidi() failed: is nullptr");
	}
}

SamplePack& ResourceManager::getKeyInstrumentSamplePack(){
	if (this->keyInstrumentSamplePack)
		return *this->keyInstrumentSamplePack;
	else {
		rt_printf("ERROR: trying getKeyInstrumentSamplePack(), but that is nullptr\n");
		throw std::runtime_error("getKeyInstrumentSamplePack() failed: is nullptr");
	}
}
SamplePack& ResourceManager::getDrumSamplePack(){
	if (this->drumSamplePack)
		return *this->drumSamplePack;
	else {
		rt_printf("ERROR: trying getDrumSamplePack(), but that is nullptr\n");
		throw std::runtime_error("getDrumSamplePack() failed: is nullptr");
	}
}

LooperLights& ResourceManager::getLooperLights(){
	if (this->looperLights)
		return *this->looperLights;
	else {
		rt_printf("ERROR: trying getLooperLights(), but that is nullptr\n");
		throw std::runtime_error("getLooperLights() failed: is nullptr");
	}
}

//TODO: remove this whole testSampleThing
std::vector<float>* ResourceManager::makeTestSample(){
	testSample = new std::vector<float>();
	int sampleSize = 3 * 44100;
	for(int i = 0; i < sampleSize; i++){
		testSample->push_back((float)i / sampleSize * sinf(2 * M_PI * (220 + i * (float)220 / sampleSize) * (float)i/44100));
	}
	testSampleSize = sampleSize;
	return testSample;
}
std::pair<float*, int> ResourceManager::getTestSample(){
	if (testSample)
		return {testSample->data(), testSampleSize};
	else {
		rt_printf("ERROR: trying to get testSample pointer, but that is nullptr\n");
		throw std::runtime_error("getTestSample() failed: testSample is nullptr");
	}
}
std::vector<float>* ResourceManager::getTestSampleVector(){
	if (testSample)
		return testSample;
	else {
		rt_printf("ERROR: trying to get testSample pointer, but that is nullptr\n");
		throw std::runtime_error("getTestSampleVector() failed: testSample is nullptr");
	}
}

std::atomic<bool>& ResourceManager::getUpdateDisplayFlag() {
	return updateDisplayFlag;
}
