#pragma once
#include <Bela.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <cmath>

#include "../general/Metronome.h"
#include "Mixer.h"
#include "SamplePack.h"
#include "Loopers.h"
#include "Samplers.h"
#include "Effects.h"

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

    EffectsChain& getInputEffects(int channel); // 0 = left, 1 = right
    EffectsChain& getInstrumentEffects();
    EffectsChain& getLooperEffects(int looperIndex);
    EffectsChain& getAllLoopersEffects();

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

    EffectsChain inputEffectsL;
    EffectsChain inputEffectsR;
    EffectsChain instrumentEffects;
    EffectsChain allLoopersEffects;
    std::vector<EffectsChain> looperEffects;
    float lastEffectsTempoBpm = -1.0f;
};
