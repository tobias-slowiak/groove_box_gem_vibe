#pragma once

#include<Bela.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "../streamingBuffer/StreamingBuffer.h"
#include "../general/TaskWrapper.h"
#include "Voices.h"
//compile
class StreamingBuffer;
using SampleIdentifier = std::pair<int, int>;
class ResourceManager;
class SamplePack {
public:
    // make one for an instrument samplepack (larger) and one for a drum sample pack (smaller)
    SamplePack(ResourceManager& resourceManager,
        std::string samplePackName, std::string samplePackFolderPath,
        size_t bufferSizeInFrames);

    void initForFolder(std::string samplePackFolderPath);

    void taskWorkMessage(std::string& taskName, DefaultTaskMessage msg);

    void initWork();

    std::unordered_map<SampleIdentifier, size_t>& getAvailableSamples();

    void initializeBuffer();

    std::vector<std::string> listWavFiles();

    //the two following methods are only for the bandwidth test
    bool streamerIsInFlight(){return streamingBuffer.streamerIsInFlight();}
    void streamFullSample(SampleIdentifier sampleIdentifier){streamingBuffer.streamFullSample(sampleIdentifier);}

    SampleIdentifier findClosestSample(SampleIdentifier sampleIdentifier);

    void triggerVoice(int note, int midiVelocity, bool gainFromVelocity = false, float explicitGain = 1.0f);

    void triggerOff(int note);

    int midiToSampleVelocity(int midiVelocity);

    void processBlockwise();

    float process();

    void printBufferInfo(){streamingBuffer.printInfo();}

private:
    Voices voices;
    std::string samplePackName;
    std::string samplePackFolderPath;
    std::unordered_map<SampleIdentifier, size_t> availableSamples;
    int maxAvailableVelocity = 0; // depends on current available Samples
    std::unordered_set<int> availableKeys;
    StreamingBuffer streamingBuffer;

    int initTaskPrio = 70;
    std::string initTaskName;
    TaskWrapper<SamplePack, DefaultTaskMessage> initTask;
};
