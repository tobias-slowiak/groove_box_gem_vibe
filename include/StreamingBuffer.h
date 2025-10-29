#include <Bela.h>
#include <vector>
#include <string>
#include <map>
#include <atomic>
#include <libraries/AudioFile/AudioFile.h>

//we have 3 different types of indices: chunkIndex (within the sample it has chunk number 0 (start) 1 2 ...)
// then we have the chunkStartIndex (which is an actual Index in the big buffer) and marks the first index of the chunk
// and we have the globalChunkIndex (which is the index of the chunk in the big buffer. 0 would be the first chunk in
//  the streamed chunk area, 1 the second, etc.

//std::pair<int,int> sampleIdentifier is either (key, velocity) or (loopIndex, barIndex) or (samplerIndex, samplerSlice)

class ResourceManager;
class Request;

class StreamingBuffer {
public:

    StreamingBuffer(ResourceManager* resourceManager,
        size_t size, std::string bufferName, std::string folderPath);

    float* getData(){ return buffer.data();}

    float at(size_t index){ return buffer.at(index);}

    void initForFolder(std::string folderPath, size_t chunkLength = kDefaultChunkLength);

    size_t requestSample(std::pair<int,int> sampleIdentifier); // returns the requestId

    void processBlockwise();

    void eraseSample(std::pair<int,int> sampleIdentifier);

    float getNextSample(size_t requestId);

    int getRequestSampleLength(size_t requestId);

    void streamStarts();

    void streamChunks();

    void streamSamples();

    void clearStreamingChunks();

    void clearContainers();

    void printInfo() const;

private:

    friend class Request;

    ResourceManager* resourceManager;
    std::string bufferName; //for example KeySamplePackBuffer or LoopersBuffer
    std::string folderPath;

    AuxiliaryTask streamSamplesTask;
    int sampleStreamPrio = 50;
    bool streamStartsNeedsScheduling = false;
    bool streamChunksNeedsScheduling = false;

    std::vector<float> buffer;
    size_t availableBufferLength; //largest possible multiple of chunkLength is available
    size_t streamedChunkAreaStartIndex; //seperates buffer from startChunks to streamedChunks.

    std::map<std::pair<int,int>, std::string> filenames;
    std::vector<std::pair<int,int>> availableSamples;
    std::vector<std::pair<int,int>> pendingSamplesToLoad;
    std::vector<std::pair<int,int>> fullyLoadedSamples;
    std::map<std::pair<int,int>, size_t> sampleLengths;
    std::vector<std::pair<int,int>> streamedChunkOwnership;

    std::map<std::pair<int,int>, std::vector<size_t>> chunkStartIndices;

    std::map<size_t, Request> activeRequests;
    size_t currentRequestId = 0;
    
    std::map<std::pair<int,int>, size_t> sdCardReadIndices;

    static constexpr size_t kDefaultChunkLength = 44100 * 0.2;
    size_t chunkLength = 44100 * 0.2; // in samples
};


class Request{
public:
    Request() {}
    Request(ResourceManager* resourceManager, std::pair<int,int> sampleIdentifier, StreamingBuffer* buffer, size_t requestId):
                requestId(requestId), resourceManager(resourceManager),
                sampleIdentifier(sampleIdentifier), buffer(buffer),
                chunkStartIndex(buffer->chunkStartIndices[sampleIdentifier].at(0)){}

    float getNextSample();

    std::pair<int, int> getSampleIdentifier() {return sampleIdentifier;}

private:

    friend class StreamingBuffer;

    size_t requestId;

    ResourceManager* resourceManager;
    std::pair<int,int> sampleIdentifier = {-1,-1};
    StreamingBuffer* buffer;
    size_t readIndexInChunk = 0;
    size_t chunkIndex = 0;
    size_t chunkStartIndex;
};
