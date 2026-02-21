#pragma once
#include <cstdint>
#include "../audio/SamplePack.h"

class ResourceManager;
class Voices;

class Metronome{
public:
    Metronome(ResourceManager& resourceManager);

    float getBPM() { return bpm; }
    void setBPM(float bpm);
    int getBeatsElapsed() { return beatsElapsed; }
    uint64_t getFrameCounter() { return frameCounter; }
    int getFramesPerBeat() {return framesPerBeat;}
    int getBeatsPerBar() { return beatsPerBar; }
    bool didBarStartThisFrame() const { return barStartedThisFrame; }
    void setBeatsPerBar(int beatsPerBar);
    int getBeatUnit() { return beatUnit; }
    void setBeatUnit(int beatUnit);

    void printBufferInfo();

    void processBlockwise();
    
    float process();

private:
    ResourceManager& resourceManager;
    SamplePack samplePack; //TODO: give it it's own sample pack?
    int samplePackSize = 1;
    float bpm = 120;
    int framesPerBeat;
    int beatsPerBar = 4;
    int beatUnit = 4;
    int beatsElapsed = 0;
    int frameCounter;
    bool barStartedThisFrame = false;
};
