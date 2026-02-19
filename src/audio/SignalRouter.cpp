
#include "../../include/audio/SignalRouter.h"
#include "../../include/general/ResourceManager.h"

SignalRouter::SignalRouter(ResourceManager& rm)
    : resourceManager(rm),
      metronome(rm.getMetronome()),
      instrumentSamplePack(rm.getKeyInstrumentSamplePack()),
      drumSamplePack(rm.getDrumSamplePack()),
      loopers(rm.getLoopers()),
      samplers(rm.getSamplers()),
      mixer(rm.getMixer()),
      inputEffectsL(static_cast<float>(rm.audioFramesPerSecond)),
      inputEffectsR(static_cast<float>(rm.audioFramesPerSecond)),
      instrumentEffects(static_cast<float>(rm.audioFramesPerSecond)),
      allLoopersEffects(static_cast<float>(rm.audioFramesPerSecond))
{
    readOnlySignals = {Signal::AudioInL,
                        Signal::AudioInR,
                        Signal::AnalogIn1,
                        Signal::AnalogIn2,
                        Signal::Metronome,
                        Signal::Instrument,
                        Signal::Drums};
    for(int i = 0; i < static_cast<int>(Signal::COUNT); ++i) {
        routingMatrix.push_back(std::vector<bool>(static_cast<int>(Signal::COUNT), false));
    }
    for(int i = 0; i < static_cast<int>(Signal::COUNT); ++i) {
        outputMatrix.push_back(std::vector<bool>(static_cast<int>(Output::COUNT), false));
    }

    inputFrames.resize(static_cast<int>(Signal::COUNT), 0.0f);
    looperEffects.reserve(static_cast<size_t>(loopers.size()));
    for(int i = 0; i < loopers.size(); ++i){
        looperEffects.emplace_back(static_cast<float>(resourceManager.audioFramesPerSecond));
    }

    // DEFAULT ROUTINGS
    setPath(Signal::Instrument, Signal::Loopers, true);
    setPath(Signal::Drums, Signal::Loopers, true);
    //Audio In feeds Noise to loopers -> disable for now
    //setPath(Signal::AudioInL, Signal::Loopers, true);
    //setPath(Signal::AudioInR, Signal::Loopers, true);
    setPath(Signal::AudioInL, Signal::Samplers, true);
    setPath(Signal::AudioInR, Signal::Samplers, true);

    //DEFAULT OUTPUTS
    for(int i = 0; i < 2; ++i) {
        Output mainOutput = static_cast<Output>(i);
        setOutput(Signal::Metronome, mainOutput, true);
        setOutput(Signal::Instrument, mainOutput, true);
        setOutput(Signal::Drums, mainOutput, true);
        setOutput(Signal::Loopers, mainOutput, true);
        setOutput(Signal::Samplers, mainOutput, true);   
    } 
}



float SignalRouter::process(int n)
{
    const float currentTempoBpm = metronome.getBPM();
    if(std::fabs(currentTempoBpm - lastEffectsTempoBpm) > 0.001f){
        inputEffectsL.setTempoBpm(currentTempoBpm);
        inputEffectsR.setTempoBpm(currentTempoBpm);
        instrumentEffects.setTempoBpm(currentTempoBpm);
        allLoopersEffects.setTempoBpm(currentTempoBpm);
        for(size_t looperIndex = 0; looperIndex < looperEffects.size(); ++looperIndex){
            looperEffects.at(looperIndex).setTempoBpm(currentTempoBpm);
        }
        lastEffectsTempoBpm = currentTempoBpm;
    }

    // ------------------------  Process Read only Signals
    inputFrames[(int)Signal::Metronome] = mixer.getGain(GainId::Metronome) * metronome.process();
    inputFrames[(int)Signal::Instrument] =  mixer.getGain(GainId::Instrument) * instrumentSamplePack.process();
    inputFrames[(int)Signal::Drums] = mixer.getGain(GainId::Drums) * drumSamplePack.process();
    if(resourceManager.keysInMelodicMode){
        inputFrames[(int)Signal::Instrument] = instrumentEffects.processSample(inputFrames[(int)Signal::Instrument]);
    } else {
        inputFrames[(int)Signal::Drums] = instrumentEffects.processSample(inputFrames[(int)Signal::Drums]);
    }
    inputFrames[(int)Signal::AudioInL] =  mixer.getGain(GainId::AudioInL) * 
                            audioRead(resourceManager.getBelaContext(), n, 0);
    inputFrames[(int)Signal::AudioInL] = inputEffectsL.processSample(inputFrames[(int)Signal::AudioInL]);
    inputFrames[(int)Signal::AudioInR] = mixer.getGain(GainId::AudioInR) * 
                            audioRead(resourceManager.getBelaContext(), n, 1);
    inputFrames[(int)Signal::AudioInR] = inputEffectsR.processSample(inputFrames[(int)Signal::AudioInR]);
    /////////!!!!!!!!!!!TODO::: make analog in right this is just a placeholder/provisorium
    inputFrames[(int)Signal::AnalogIn1] = mixer.getGain(GainId::AnalogIn1) * 
                            analogRead(resourceManager.getBelaContext(), n, 0);
    inputFrames[(int)Signal::AnalogIn2] = mixer.getGain(GainId::AnalogIn2) * 
                            analogRead(resourceManager.getBelaContext(), n, 1);
    // ------------------------ Process Read Write Signals
    float looperInputFrame = 0.0f;
    float samplerInputFrame = 0.0f;
    for(int signalIndex =0; signalIndex < static_cast<int>(Signal::COUNT); ++signalIndex) {
        looperInputFrame += routingMatrix[signalIndex][(int)Signal::Loopers] * inputFrames[signalIndex];
        samplerInputFrame += routingMatrix[signalIndex][(int)Signal::Samplers] * inputFrames[signalIndex];
    }
    inputFrames[(int)Signal::Loopers] = 0.0f;
    std::vector<float>& looperOutput = loopers.process(looperInputFrame);
    for(size_t looperIndex = 0; looperIndex < looperOutput.size(); looperIndex++){
        float processedLooperFrame = looperOutput.at(looperIndex);
        if(looperIndex < looperEffects.size()){
            processedLooperFrame = looperEffects.at(looperIndex).processSample(processedLooperFrame);
        }
        inputFrames[(int)Signal::Loopers] += processedLooperFrame * mixer.getLooperGain(looperIndex);
    }
    inputFrames[(int)Signal::Loopers] = allLoopersEffects.processSample(inputFrames[(int)Signal::Loopers]);
    inputFrames[(int)Signal::Loopers] *= mixer.getGain(GainId::Looper);
    inputFrames[(int)Signal::Samplers] = mixer.getGain(GainId::Sampler) * samplers.process(samplerInputFrame);
    // ------------------------ Mix to Outputs
    float mainOutMix = 0.0f;
    for(int outputIndex = 0; outputIndex < static_cast<int>(Output::COUNT); ++outputIndex) {
        Output output = static_cast<Output>(outputIndex);
        float mixedFrame = 0.0f;
        for(int signalIndex = 0; signalIndex < static_cast<int>(Signal::COUNT); ++signalIndex) {
            mixedFrame += inputFrames[signalIndex] * outputMatrix[signalIndex][outputIndex];
        }
        mixedFrame *= mixer.getGain(GainId::Master);
        switch (output)
        {
        case Output::Main1:
            mainOutMix = mixedFrame;
            audioWrite(resourceManager.getBelaContext(), n, 0, mixedFrame);
            break;
        case Output::Main2:
            audioWrite(resourceManager.getBelaContext(), n, 1, mixedFrame);
            break;
        default:
            break;
        }
    }
    return mainOutMix;
}

EffectsChain& SignalRouter::getInputEffects(int channel){
    if(channel == 0){
        return inputEffectsL;
    }
    if(channel == 1){
        return inputEffectsR;
    }
    throw std::runtime_error("SignalRouter::getInputEffects invalid channel " + std::to_string(channel));
}

EffectsChain& SignalRouter::getInstrumentEffects(){
    return instrumentEffects;
}

EffectsChain& SignalRouter::getLooperEffects(int looperIndex){
    if(looperIndex < 0 || static_cast<size_t>(looperIndex) >= looperEffects.size()){
        throw std::runtime_error("SignalRouter::getLooperEffects invalid looperIndex " + std::to_string(looperIndex));
    }
    return looperEffects.at(looperIndex);
}

EffectsChain& SignalRouter::getAllLoopersEffects(){
    return allLoopersEffects;
}

bool SignalRouter::getPath(Signal from, Signal to)
{
    int fromIndex = static_cast<int>(from);
    int toIndex = static_cast<int>(to);
    if(fromIndex >= 0 && toIndex >= 0 &&
       fromIndex < routingMatrix.size() && toIndex < routingMatrix.size()) {
        return routingMatrix[fromIndex][toIndex];
    } else {
        throw std::runtime_error("SignalRouter::getOutput: Invalid signal or output: signal " +
                                 std::to_string(fromIndex) + " output " + std::to_string(toIndex));
    }
}

void SignalRouter::setPath(Signal from, Signal to, bool enabled)
{
    int fromIndex = static_cast<int>(from);
    int toIndex = static_cast<int>(to);
    if(fromIndex == toIndex) {
        DEBUG_RT_PRINTF("WARNING: Attempt to setPath from a signal to itself %d\n", static_cast<int>(from));
        return;
    }
    if(readOnlySignals.count(to) > 0) {
        DEBUG_RT_PRINTF("WARNING: Attempt to write to readOnly signal %d\n", static_cast<int>(to));
        return;
    }
    if(fromIndex >= 0 && toIndex >= 0 && fromIndex < routingMatrix.size() && toIndex < routingMatrix.size()) {
        routingMatrix[fromIndex][toIndex] = enabled;
    } else {
        throw std::runtime_error("SignalRouter::setPath: Invalid signal: from " + std::to_string(fromIndex) + " to " + std::to_string(toIndex));
    }
}

void SignalRouter::setOutput(Signal signal, Output output, bool enabled)
{
    int signalIndex = static_cast<int>(signal);
    int outputIndex = static_cast<int>(output);
    if(signalIndex >= 0 && outputIndex >= 0 &&
       signalIndex < outputMatrix.size() && outputIndex < outputMatrix.size()) {
        outputMatrix[signalIndex][outputIndex] = enabled;
    } else {
        throw std::runtime_error("SignalRouter::setOutput: Invalid signal or output: signal " +
                                 std::to_string(signalIndex) + " output " + std::to_string(outputIndex));
    }
}

bool SignalRouter::getOutput(Signal signal, Output output)
{
    int signalIndex = static_cast<int>(signal);
    int outputIndex = static_cast<int>(output);
    if(signalIndex >= 0 && outputIndex >= 0 &&
       signalIndex < outputMatrix.size() && outputIndex < outputMatrix.size()) {
        return outputMatrix[signalIndex][outputIndex];
    } else {
        throw std::runtime_error("SignalRouter::getOutput: Invalid signal or output: signal " +
                                 std::to_string(signalIndex) + " output " + std::to_string(outputIndex));
    }
}

SignalRouter::~SignalRouter()
{
}
