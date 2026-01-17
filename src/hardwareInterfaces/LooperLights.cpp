#include "../../include/hardwareInterfaces/LooperLights.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/audio/Loopers.h"
#include "../../include/hardwareInterfaces/DeviceMap.h"
#include "../../include/hardwareInterfaces/MidiReal.h"

LooperLights::LooperLights(ResourceManager& resourceManager)
    : resourceManager(resourceManager),
      loopers(resourceManager.getLoopers()),
      deviceMap(resourceManager.getDeviceMap())
{
}

void LooperLights::initialize() {
    for(auto& buttonIndexPair : deviceMap.looperToPlayControl) {
        turnOffPlayingLight(buttonIndexPair.first);
    }
    for(auto& buttonIndexPair : deviceMap.looperToRecControl) {
        turnOffRecordingLight(buttonIndexPair.first);
    }
}

void LooperLights::update() {
    int nrLoopers = loopers.getNrLoopers();
    for(int i = 0; i < nrLoopers; i++) {
        if(loopers.isPlaying(i)){
            turnOnPlayingLight(i);
        } else {
            turnOffPlayingLight(i);
        }
        if(loopers.isRecording(i)){
            turnOnRecordingLight(i);
        } else {
            turnOffRecordingLight(i);
        }
        //Order important: if recording, waiting light overrules
        if(loopers.isWaitingForBarStart(i)){
            turnOnWaitingForBarStartLight(i);
        } else {
            turnOffWaitingForBarStartLight(i);
            if(loopers.isRecording(i)){
                //this generally should not happen, but just in case
                turnOnRecordingLight(i);
            }
        }
    }
}

void LooperLights::turnOnPlayingLight(int looperIndex) {
    #ifdef LAUNCHKEY_46_MK1
        IMidi& midi = resourceManager.getControlMidi();
        midi.writeMessage(resourceManager.getDeviceMap().LED_STATUS_BYTE,
                        resourceManager.getDeviceMap().LED_SOLID_ON_CHANNEL,
                        resourceManager.getDeviceMap().looperToPlayControl.at(looperIndex),
                        resourceManager.getDeviceMap().LED_GREEN);
    #endif
}

void LooperLights::turnOffPlayingLight(int looperIndex) {
    #ifdef LAUNCHKEY_46_MK1
        IMidi& midi = resourceManager.getControlMidi();
        midi.writeMessage(resourceManager.getDeviceMap().LED_STATUS_BYTE,
                        resourceManager.getDeviceMap().LED_SOLID_ON_CHANNEL,
                        resourceManager.getDeviceMap().looperToPlayControl.at(looperIndex),
                        resourceManager.getDeviceMap().LED_OFF);
    #endif
}

void LooperLights::turnOnRecordingLight(int looperIndex) {
    #ifdef LAUNCHKEY_46_MK1
        IMidi& midi = resourceManager.getControlMidi();
        midi.writeMessage(resourceManager.getDeviceMap().LED_STATUS_BYTE,
                        resourceManager.getDeviceMap().LED_SOLID_ON_CHANNEL,
                        resourceManager.getDeviceMap().looperToRecControl.at(looperIndex),
                        resourceManager.getDeviceMap().LED_RED);
    #endif
}

void LooperLights::turnOffRecordingLight(int looperIndex) {
    #ifdef LAUNCHKEY_46_MK1
        IMidi& midi = resourceManager.getControlMidi();
        midi.writeMessage(resourceManager.getDeviceMap().LED_STATUS_BYTE,
                        resourceManager.getDeviceMap().LED_SOLID_ON_CHANNEL,
                        resourceManager.getDeviceMap().looperToRecControl.at(looperIndex),
                        resourceManager.getDeviceMap().LED_OFF);
    #endif
}

void LooperLights::turnOnWaitingForBarStartLight(int looperIndex) {
    #ifdef LAUNCHKEY_46_MK1
        IMidi& midi = resourceManager.getControlMidi();
        midi.writeMessage(resourceManager.getDeviceMap().LED_STATUS_BYTE,
                        resourceManager.getDeviceMap().LED_SOLID_ON_CHANNEL,
                        resourceManager.getDeviceMap().looperToRecControl.at(looperIndex),
                        resourceManager.getDeviceMap().LED_RED);
        midi.writeMessage(resourceManager.getDeviceMap().LED_STATUS_BYTE,
                        resourceManager.getDeviceMap().LED_FLASHING_ON_CHANNEL,
                        resourceManager.getDeviceMap().looperToRecControl.at(looperIndex),
                        resourceManager.getDeviceMap().LED_RED);
    #endif
}

void LooperLights::turnOffWaitingForBarStartLight(int looperIndex) {
    #ifdef LAUNCHKEY_46_MK1
        IMidi& midi = resourceManager.getControlMidi();
        midi.writeMessage(resourceManager.getDeviceMap().LED_STATUS_BYTE,
                        resourceManager.getDeviceMap().LED_FLASHING_ON_CHANNEL,
                        resourceManager.getDeviceMap().looperToRecControl.at(looperIndex),
                        resourceManager.getDeviceMap().LED_OFF);
    #endif
}