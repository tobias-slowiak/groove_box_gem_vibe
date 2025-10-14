#include <vector>
#include <algorithm>
#include <fcntl.h> 
#include <sys/ioctl.h>      
#include <linux/i2c-dev.h> 
#include <unistd.h> 
#include <cstdio>  
#include <cerrno>  
#include <stdio.h>

#include "../include/ResourceManager.h"
#include "../include/ModeManager.h"
#include "../include/DeviceMap.h"
#include "../include/BelaInterface.h"
#include "../include/Controller.h"
#include "../include/Voices.h"
#include "../include/Samplers.h"
#include "../include/Loopers.h"
#include "../include/IDisplayContext.h"
#include "../include/DisplayContextReal.h"
#include "../include/DisplayContextFake.h"
#include "../include/IMidi.h"
#include "../include/MidiReal.h"
#include "../include/MidiFake.h"



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
	printf("------------------------------------\n");
	printf("-----ResourceManager setup----------\n");
	this->context = context;
	modeManager = new ModeManager(this);
	
	//External devices 
	deviceMap = new DeviceMap(this);

	//Interface
	interfaceConnected = i2cDevicePresent(1, 0x3c);

	if(interfaceConnected){
		printf("Interface Connected\n");
		interface = new BelaInterface(this);
		std::vector<U8G2*> u8g2s = initU8G2s();
	    displayContext = new DisplayContextReal(u8g2s, updateDisplayFlag);
	    displayContext->initDisplayContext();
	    updateDisplayFlag = false;
	} else {
		printf("Interface not Connected\n");
		displayContext = new DisplayContextFake();
	}

    //Midi
    keyMidi = new MidiReal();
	keyMidiConnected = (keyMidi->readFrom(deviceMap->keyMidiName.c_str()) == 1);
	if(keyMidiConnected){
		printf("Key Midi Device available.\n");
		keyMidi->enableParser(true);
	} else {
		printf("Key Midi Device not available\n");
		delete keyMidi;
		keyMidi = new MidiFake();
	}

	controlMidi = new MidiReal();
	controlMidiConnected = (controlMidi->readFrom(deviceMap->controlMidiName.c_str()) == 1);
	if(keyMidiConnected){
		printf("Control Midi Device available.\n");
		controlMidi->writeTo(deviceMap->controlMidiName.c_str());
    	controlMidi->enableParser(true);
	} else {
		printf("Control Midi Device not available\n");
		delete controlMidi;
		controlMidi = new MidiFake();
	}

	//Constants
	audioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
	audioFramesPerSecond = context->audioSampleRate;
	audioInputChannels = context->audioInChannels;
	audioFramesPerBlock = context->audioFrames;
	
	//Classes
	controller = new Controller(this);
	voices = new Voices(this);
	printf("Constructed Vocies\n");
	samplers = new Samplers(this);
	printf("Constructed Samplers\n");
	loopers = new Loopers(this);
	printf("Constructed Loopers\n");
	
	this->makeTestSample();
	printf("Made TestSample\n");
	printf("------------------------------------\n");
}

void ResourceManager::cleanup(BelaContext* context){
	delete modeManager;
	delete deviceMap;
	if(interfaceConnected) delete interface;
	delete displayContext;
	delete keyMidi;
	delete controlMidi;
	delete voices;
	delete samplers;
	delete loopers;
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

ModeManager* ResourceManager::getModeManager(){
	if (modeManager)
		return modeManager;
	else {
		rt_printf("ERROR: trying to getModeManager(), but that is nullptr\n");
		throw std::runtime_error("getModeManager() failed: is nullptr");
	}
}

DeviceMap* ResourceManager::getDeviceMap(){
	if (deviceMap)
		return deviceMap;
	else {
		rt_printf("ERROR: trying to getDeviceMap(), but that is nullptr\n");
		throw std::runtime_error("getDeviceMap() failed: is nullptr");
	}
}

IDisplayContext* ResourceManager::getDisplayContext(){
	if (displayContext)
		return displayContext;
	else {
		rt_printf("ERROR: trying to getDisplayContext(), but that is nullptr\n");
		throw std::runtime_error("getDisplayContext() failed: is nullptr");
	}
}

Samplers* ResourceManager::getSamplers() {
	if (samplers)
		return samplers;
	else {
		rt_printf("ERROR: trying to getSamplers(), but that is nullptr\n");
		throw std::runtime_error("getSamplers() failed: is nullptr");
	}
}

Voices* ResourceManager::getVoices(){
	if (voices)
		return voices;
	else {
		rt_printf("ERROR: trying to getVoices(), but that is nullptr\n");
		throw std::runtime_error("getVoices() failed: is nullptr");
	}
}

Loopers* ResourceManager::getLoopers(){
	if (loopers)
		return loopers;
	else {
		rt_printf("ERROR: trying togetLoopers(), but that is nullptr\n");
		throw std::runtime_error("getLoopers() failed: is nullptr");
	}
}

Controller* ResourceManager::getController(){
	if (controller)
		return controller;
	else {
		rt_printf("ERROR: trying getController(), but that is nullptr\n");
		throw std::runtime_error("getController() failed: is nullptr");
	}
}

BelaInterface* ResourceManager::getBelaInterface(){
	if (interface)
		return interface;
	else {
		rt_printf("ERROR: trying getBelaInterface(), but that is nullptr\n");
		throw std::runtime_error("getBelaInterface() failed: is nullptr");
	}
}

IMidi* ResourceManager::getKeyMidi(){
	if (this->keyMidi)
		return this->keyMidi;
	else {
		rt_printf("ERROR: trying getKeyMidi(), but that is nullptr\n");
		throw std::runtime_error("getKeyMidi() failed: is nullptr");
	}
}

IMidi* ResourceManager::getControlMidi(){
	if (this->controlMidi)
		return this->controlMidi;
	else {
		rt_printf("ERROR: trying getControlMidi(), but that is nullptr\n");
		throw std::runtime_error("getControlMidi() failed: is nullptr");
	}
}


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
void ResourceManager::updateDisplay(){
	updateDisplayFlag = true;
}



