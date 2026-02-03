#include "../../include/general/ResourceManager.h"
#include "../../include/audio/Mixer.h"
#include "../../include/hardwareInterfaces/BelaInterface.h"
//compile

void ResourceManager::processBlockwise(){
    displayContext->processBlockwise();
    interface->processBlockwise();
    inputHandler->parseMessages();
    assert(keyInstrumentSamplePack);
    keyInstrumentSamplePack->processBlockwise();
    drumSamplePack->processBlockwise();
    metronome->processBlockwise();
    ui->processBlockwise();

    for(unsigned int n = 0; n < audioFramesPerBlock; n++) {
        float mainOutMix = signalRouter->process(n);
        recorder->process(mainOutMix);
    }
}