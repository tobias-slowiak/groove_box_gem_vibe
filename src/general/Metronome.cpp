
#include "../../include/general/ResourceManager.h"
#include "../../include/general/Metronome.h"

int bpmToFramesPerBeat(float bpm, int sampleRate, int beatUnit) {
    float res = (sampleRate * 60.0f) / bpm / (beatUnit / 4.0f);
    return (int)res;
}



Metronome::Metronome(ResourceManager& resourceManager)
                : resourceManager(resourceManager),
                samplePack(resourceManager, "metronome", "/mnt/sdcard/Samples/metronomeSets/hiHat808", 44100 * 5) {  // 5 seconds buffer should be plenty
    framesPerBeat = bpmToFramesPerBeat(bpm, resourceManager.audioFramesPerSecond, beatUnit);
    frameCounter = 0;
}

void Metronome::setBPM(float bpm) {
    this->bpm = bpm;
    framesPerBeat = bpmToFramesPerBeat(bpm, resourceManager.audioFramesPerSecond, beatUnit);
}

void Metronome::setBeatsPerBar(int beatsPerBar) {
    if (beatsPerBar < 1) {
        beatsPerBar = 1;
    }
    this->beatsPerBar = beatsPerBar;
    if (beatsElapsed >= this->beatsPerBar) {
        beatsElapsed = 0;
    }
}

void Metronome::setBeatUnit(int beatUnit) {
    if (beatUnit < 1) {
        beatUnit = 1;
    }
    this->beatUnit = beatUnit;
    framesPerBeat = bpmToFramesPerBeat(bpm, resourceManager.audioFramesPerSecond, this->beatUnit);
}

void Metronome::printBufferInfo() {
    samplePack.printBufferInfo();
}

void Metronome::processBlockwise() {
    samplePack.processBlockwise();
}

float Metronome::process() {
    bool beatchanged = false;
    frameCounter++;
    if(frameCounter >= framesPerBeat) {
        frameCounter = 0; // Reset for new bar
        beatsElapsed++;
        beatchanged = true;
        if(beatsElapsed >= beatsPerBar){
            if(samplePackSize == 1)
                samplePack.triggerVoice(42, 64, false, 1.0f);
            else
                samplePack.triggerVoice(44, 64, false, 1.0f);
            beatsElapsed = 0;
            resourceManager.getUI().updateDisplay(); // if in UIstate loopers, update bar count
        } else {
            samplePack.triggerVoice(42, 64, false, 0.5f); //Click sound for main out
        }
    }
    if(beatchanged)
    return samplePack.process();
}
