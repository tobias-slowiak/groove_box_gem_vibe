#include "../../include/hardwareInterfaces/LooperLights.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/audio/Loopers.h"
#include "../../include/hardwareInterfaces/DeviceMap.h"
#include "../../include/hardwareInterfaces/MidiReal.h"

LooperLights::LooperLights(ResourceManager& resourceManager)
    : resourceManager(resourceManager),
      deviceMap(resourceManager.getDeviceMap()),
      midi(resourceManager.getControlMidi())
{
}

void LooperLights::initialize() {
    midi.writeMessage(deviceMap.midiControlChange,
        deviceMap.midiSetDrumModeChannel,
        deviceMap.midiSetDrumModeByte1,
        deviceMap.midiSetDrumModeByte2);
    for(auto& looperIndexLightIndex : deviceMap.looperToPlayControl) {
        midi.writeMessage(deviceMap.LED_STATUS_BYTE,
            deviceMap.LED_SOLID_ON_CHANNEL,
            looperIndexLightIndex.second,
            deviceMap.LED_OFF);
    }
    for(auto& looperIndexLightIndex : deviceMap.looperToRecControl) {
        midi.writeMessage(deviceMap.LED_STATUS_BYTE,
            deviceMap.LED_SOLID_ON_CHANNEL,
            looperIndexLightIndex.second,
            deviceMap.LED_OFF);
    }
}

void LooperLights::setLight(LooperLightMessage msg, int looperIndex) {
    std::string debugStr;
    switch(msg) {
        case LooperLightMessage::PlayingOn:
            midi.writeMessage(deviceMap.LED_STATUS_BYTE,
                deviceMap.LED_SOLID_ON_CHANNEL,
                VEC_AT(deviceMap.looperToPlayControl, looperIndex),
                deviceMap.LED_GREEN);
            debugStr = "PlayingOn";
            break;
        case LooperLightMessage::PlayingOff:
            midi.writeMessage(deviceMap.LED_STATUS_BYTE,
                deviceMap.LED_SOLID_ON_CHANNEL,
                VEC_AT(deviceMap.looperToPlayControl, looperIndex),
                deviceMap.LED_OFF);
            debugStr = "PlayingOff";
            break;
        case LooperLightMessage::RecordingOn:
            midi.writeMessage(deviceMap.LED_STATUS_BYTE,
                deviceMap.LED_SOLID_ON_CHANNEL,
                VEC_AT(deviceMap.looperToRecControl, looperIndex),
                deviceMap.LED_RED);
            debugStr = "RecordingOn";
            break;
        case LooperLightMessage::RecordingOff:
            midi.writeMessage(deviceMap.LED_STATUS_BYTE,
                deviceMap.LED_SOLID_ON_CHANNEL,
                VEC_AT(deviceMap.looperToRecControl, looperIndex),
                deviceMap.LED_OFF);
            debugStr = "RecordingOff";
            break;
        case LooperLightMessage::WaitingForBarStart:
            midi.writeMessage(deviceMap.LED_STATUS_BYTE,
                deviceMap.LED_SOLID_ON_CHANNEL,
                VEC_AT(deviceMap.looperToRecControl, looperIndex),
                deviceMap.LED_OFF);
            midi.writeMessage(deviceMap.LED_STATUS_BYTE,
                deviceMap.LED_FLASHING_ON_CHANNEL,
                VEC_AT(deviceMap.looperToRecControl, looperIndex),
                deviceMap.LED_RED);
            debugStr = "WaitingForBarStart";
            break;
    }
    //rt_printf("LooperLights: set light %s for looper %d\n", debugStr.c_str(), looperIndex);
}
