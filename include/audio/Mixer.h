#pragma once

#include "../general/BasicUtilities.h"
//compile

//TODO: change name to just simply Gain
enum class GainId{
    Master,
    Instrument,
    Drums,
    Metronome,
    Looper,
    Sampler,
    AudioInL,
    AudioInR,
    AnalogIn1,
    AnalogIn2,
    COUNT
};

class ResourceManager;

class Mixer{
public:
    Mixer(ResourceManager& resourceManager);

    void setGain(GainId gainId, float value);
    void setLooperGain(int looperIndex, float value);

    float getLooperGain(int looperIndex);
    float getGain(GainId gainId);

private:
    ResourceManager& resourceManager;
    std::vector<float> gainz;
    std::vector<float> gainRanges;
    std::vector<float> looperGainz;
};