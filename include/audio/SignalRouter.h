#pragma once
#include <Bela.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

#include "../general/Metronome.h"
#include "Mixer.h"
#include "SamplePack.h"
#include "Loopers.h"
#include "Samplers.h"

class ResourceManager;


static const int MAX_LOOPERS = 16;
static const int MAX_SAMPLERS = 16;

enum class Signal {
    AudioInL,
    AudioInR,
    AnalogIn1,
    AnalogIn2,
    Metronome,
    Instrument,
    Drums,
    Loopers,
    Samplers,
    COUNT
};
enum class Output {
    Main1,
    Main2,
    Analog1,
    Analog2,
    COUNT
};

class SignalRouter
{
public:
    SignalRouter(ResourceManager& rm);
    ~SignalRouter();

    void setPath(Signal from, Signal to, bool enabled);
    bool getPath(Signal from, Signal to) ;
    void setOutput(Signal signal, Output output, bool enabled);
    bool getOutput(Signal signal, Output output) ;

    float process(int n);

private:
    std::unordered_set<Signal> readOnlySignals;
    std::vector<std::vector<bool>> routingMatrix;
    std::vector<std::vector<bool>> outputMatrix;
    std::vector<float> inputFrames;
    ResourceManager& resourceManager;
    Metronome& metronome;
    SamplePack& instrumentSamplePack;
    SamplePack& drumSamplePack;
    Loopers& loopers;
    Samplers& samplers;
    Mixer& mixer;
};
