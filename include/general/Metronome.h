#pragma once
#include <cstdint>

class ResourceManager;
class Voices;
class SamplePack;

class Metronome{
public:
    Metronome(ResourceManager& resourceManager);

    float getBPM() { return bpm; }
    void setBPM(float bpm);
    int getBeatsElapsed() { return beatsElapsed; }
    int getBeatsPerBar() { return beatsPerBar; }
    void setBeatsPerBar(int beatsPerBar);
    int getBeatUnit() { return beatUnit; }
    void setBeatUnit(int beatUnit);

    void mainOutonOffToggle();
    bool mainOutIsOn() { return mainOutIsOnFlag; }
    void analogOutonOffToggle();
    bool analogOutIsOn() { return analogOutIsOnFlag; }

    void process();

private:
    ResourceManager& resourceManager;
    SamplePack& samplePack; //TODO: give it it's own sample pack?
    float bpm = 120;
    float framesPerBeat;
    int beatsPerBar = 4;
    int beatUnit = 4;
    int beatsElapsed = 0;
    uint64_t frameCounter;
    bool mainOutIsOnFlag = true;
    bool analogOutIsOnFlag = true;
};