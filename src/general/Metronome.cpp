
#include "../../include/general/ResourceManager.h"
#include "../../include/general/Metronome.h"

float bpmToFramesPerBeat(float bpm, int sampleRate, int beatUnit) {
    return (sampleRate * 60.0f) / bpm / (beatUnit / 4.0f);
}
Metronome::Metronome(ResourceManager& resourceManager): resourceManager(resourceManager), samplePack(resourceManager.getDrumSamplePack()) {
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

void Metronome::mainOutonOffToggle() {
    mainOutIsOnFlag = !mainOutIsOnFlag;
}

void Metronome::analogOutonOffToggle() {
    analogOutIsOnFlag = !analogOutIsOnFlag;
}

void Metronome::process() {
    frameCounter++;
    if(frameCounter >= framesPerBeat) {
        if(mainOutIsOnFlag) {
            samplePack.triggerVoice(37,64); //Click sound for main out
        }
        frameCounter = 0; // Reset for new bar
        beatsElapsed++;
        if(beatsElapsed >= beatsPerBar){
            if(mainOutIsOnFlag) {
                samplePack.triggerVoice(39,64); //Accented click sound for main out
            }
            beatsElapsed = 0;
        }
    }
}
