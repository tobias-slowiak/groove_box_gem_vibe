#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <Bela.h>
#include "../streamingBuffer/StreamingBuffer.h"

class ResourceManager;

class Recorder {
public:
    Recorder(ResourceManager& resourceManager, std::string filename);
    void startRecording();
    void stopRecording();
    bool isRecording(){return isRecordingFlag;}
    void process(float inFrame);

private:
    ResourceManager& resourceManager;
    std::string filename;
    std::unordered_map<SampleIdentifier, size_t> availableSamples;
    StreamingBuffer streamingBuffer;
    bool isRecordingFlag = false;
    std::vector<StreamingBufferIterator*> iterators;
    int currentSample = 0;
    int positionInSample = 0;
};
