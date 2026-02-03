#include <vector>
#include <Bela.h>
#include "../../include/general/ResourceManager.h"
#include "../../include/hardwareInterfaces/DeviceMap.h"
#include "../../include/general/BasicUtilities.h"
#include "../../include/audio/Mixer.h"

Mixer::Mixer(ResourceManager& resourceManager): resourceManager(resourceManager){
    for(int i = 0; i < (int)GainId::COUNT; i++){
        gainz.push_back(1.0f);
    }
    for(int i = 0; i < resourceManager.getDeviceMap().initialLooperNumber; i++){
        looperGainz.push_back(1.0f);
    }
}

void Mixer::setLooperGain(int looperIndex, float value){
    if(looperIndex >= (int)looperGainz.size()){
        looperGainz.resize(looperIndex + 1, 1.0f);
    }
    looperGainz[looperIndex] = value;
}
float Mixer::getLooperGain(int looperIndex){
    //TODO: mutate with new loopers
    if(looperIndex < (int)looperGainz.size()){
        return looperGainz[looperIndex];
    } else {
        return 1.0f;
    }
}

float Mixer::getGain(GainId gainId){
    return VEC_AT(gainz, (int)gainId);
}

void Mixer::setGain(GainId gainId, float value){
    VEC_AT(gainz, (int)gainId) = value;
}