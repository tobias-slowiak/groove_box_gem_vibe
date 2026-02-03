
#include "../../include/audio/SignalRouter.h"
#include "../../include/general/ResourceManager.h"

SignalRouter::SignalRouter(ResourceManager& rm)
    : resourceManager(rm),
      metronome(rm.getMetronome()),
      instrumentSamplePack(rm.getKeyInstrumentSamplePack()),
      drumSamplePack(rm.getDrumSamplePack()),
      loopers(rm.getLoopers()),
      samplers(rm.getSamplers()),
      mixer(rm.getMixer())
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
    // ------------------------  Process Read only Signals
    inputFrames[(int)Signal::Metronome] = mixer.getGain(GainId::Metronome) * metronome.process();
    inputFrames[(int)Signal::Instrument] =  mixer.getGain(GainId::Instrument) * instrumentSamplePack.process();
    inputFrames[(int)Signal::Drums] = mixer.getGain(GainId::Drums) * drumSamplePack.process();
    inputFrames[(int)Signal::AudioInL] =  mixer.getGain(GainId::AudioInL) * 
                            audioRead(resourceManager.getBelaContext(), n, 0);
    inputFrames[(int)Signal::AudioInR] = mixer.getGain(GainId::AudioInR) * 
                            audioRead(resourceManager.getBelaContext(), n, 1);
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
    inputFrames[(int)Signal::Loopers] = mixer.getGain(GainId::Looper) * loopers.process(looperInputFrame);
    //inputFrames[(int)Signal::Samplers] = mixer.getGain(GainId::Sampler) * samplers.process(samplerInputFrame);
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
        ////////////////TODO: Analog outputs must do this if n%analogFramesPerAudioFrame==0 thing
        case Output::Analog1:
            if(n % resourceManager.audioFramesPerAnalogFrame == 0)
                    analogWriteOnce(resourceManager.getBelaContext(), n/2, 5, mixedFrame);
            break;
        case Output::Analog2:
            if(n % resourceManager.audioFramesPerAnalogFrame == 0)
                    analogWriteOnce(resourceManager.getBelaContext(), n/2, 6, mixedFrame);
            break;
        default:
            break;
        }
    }
    return mainOutMix;
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
