#include "../../include/general/ResourceManager.h"
#include "../../include/audio/Mixer.h"
#include "../../include/hardwareInterfaces/BelaInterface.h"
//compile

void ResourceManager::processBlockwise(){
    displayContext->processBlockwise();
    interface->processBlockwise();
    //controller->processBlockwise();
    inputHandler->parseMessages();
    assert(keyInstrumentSamplePack);
    keyInstrumentSamplePack->processBlockwise();
    drumSamplePack->processBlockwise();
}



float ResourceManager::getNextFrame(int n){
    VEC_AT(frames, (int)GainId::Instrument) = voices->process();
    return mixer->mix(frames);
}

    /*
    //TODO voies also contains samplers should it have its own gain?
    VEC_AT(frames, (int)GainId::Instrument) = voices->process();
    VEC_AT(frames, (int)GainId::Looper) = loopers->process();
    VEC_AT(frames, (int)GainId::InputL) = audioRead(context, n, 0);
    VEC_AT(frames, (int)GainId::InputR) = audioRead(context, n, 1);
    //TODO: direct input frames into voices and loopers.
    return mixer->mix(frames);
    */