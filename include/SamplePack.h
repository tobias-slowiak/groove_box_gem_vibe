#pragma once

#include<Bela.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "StreamingBuffer.h"

class StreamingBuffer;
using SampleIdentifier = std::pair<int, int>;
class ResourceManager;
class Voices;

class SamplePack {
public:
    // make one for an instrument samplepack (larger) and one for a drum sample pack (smaller)
    SamplePack(ResourceManager* resourceManager, Voices* voices, 
        std::string samplePackName, std::string samplePackFolderPath,
        size_t bufferSizeInFrames);

    void setFolderPath(std::string folderPath){samplePackFolderPath = folderPath;}

    std::unordered_map<SampleIdentifier, size_t>& getAvailableSamples();

    void initializeBuffer();

    std::vector<std::string> listWavFiles();

    SampleIdentifier findClosestSample(SampleIdentifier sampleIdentifier);

    void triggerVoice(SampleIdentifier sampleIdentifier);

    void processBlockwise(){streamingBuffer.processBlockwise();}

    void printBufferInfo(){streamingBuffer.printInfo();}

private:
    Voices* voices;
    std::string samplePackName;
    std::string samplePackFolderPath;
    std::unordered_map<SampleIdentifier, size_t> availableSamples;
    std::unordered_set<int> availableKeys;
    StreamingBuffer streamingBuffer;
};
