#include <Bela.h>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <atomic>
#include <cstdint>
#include <libraries/AudioFile/AudioFile.h>
#include <cassert>

//we have 3 different types of indices: chunkIndex (within the sample it has chunk number 0 (start) 1 2 ...)
// then we have the chunkStartIndex (which is an actual Index in the big buffer) and marks the first index of the chunk
// and we have the globalChunkIndex (which is the index of the chunk in the big buffer. 0 would be the first chunk in
//  the streamed chunk area, 1 the second, etc.

//std::pair<int,int> sampleIdentifier is either (key, velocity) or (loopIndex, barIndex) or (samplerIndex, samplerSlice)



//TODO: make activeREquests an unordered_map for better performance. For this the definition
//of Request needs to be moved above StreamingBuffer or in it's own header.
//  in general make everything which is map an unordered_map if possible.

//TODO : make the flag into std::atomic<uint32_t> startJobId{0}; std::atomic<uint32_t> startJobDone{0};
// to avoid locking. codex said:
/*
When the audio thread queues a starts job, do const auto job = startJobId.fetch_add(1, std::memory_order_relaxed) + 1; pendingStartJob = job;.
The worker copies pendingStartJob into startJobDone.store(job, std::memory_order_release);.
ModeManager polls startJobDone.load(std::memory_order_acquire) and compares against the job id it observed when it queued the work. No chance of missing an edge, no need for extra delays, and you could extend the same scheme to chunk loads.
*/

/*
TODO: compose into smaller chunks:
codex sais the responsibilities are:
a managing big buffer
b chunk bookkeeping
c managing flags and tasks
d resource catalog (filenames, avaliable samples, ...)

*/
class ResourceManager;
class Request;

class StreamingBuffer {
public:

    StreamingBuffer(ResourceManager* resourceManager,
        size_t size, std::string bufferName, std::string folderPath);

    float* getData(){ return buffer.data();}

    float at(size_t index){
        assert(buffer.size() > index);
        return buffer.at(index);
    }

    void initForFolder(std::string folderPath, size_t chunkLength = kDefaultChunkLength);

    size_t requestSample(std::pair<int,int> sampleIdentifier); // returns the requestId

    void processBlockwise();

    void eraseSample(std::pair<int,int> sampleIdentifier);

    float getNextSample(size_t requestId);

    int getRequestSampleLength(size_t requestId);

    std::vector<std::pair<int,int>>& getAvailableSamples() {return availableSamples;}

    void streamStarts();

    void streamChunks();

    void streamSamples();

    void clearStreamingChunks();

    void clearContainers();

    void printInfo() const;

    bool isStreaming() const;

    uint32_t getStartJobsIssued() const;
    uint32_t getStartJobsCompleted() const;
    uint32_t getChunkJobsIssued() const;
    uint32_t getChunkJobsCompleted() const;

private:

    friend class Request;
    
    enum class StreamJobKind : uint8_t {
        None = 0,
        Starts,
        Chunks
    };

    enum class ScheduleStatus {
        Scheduled,
        Busy,
        Error
    };

    ScheduleStatus scheduleStreamTask(StreamJobKind kind);

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

    std::atomic<uint32_t> startJobsIssued{0};
    std::atomic<uint32_t> startJobsCompleted{0};
    std::atomic<uint32_t> chunkJobsIssued{0};
    std::atomic<uint32_t> chunkJobsCompleted{0};
    std::atomic<uint32_t> activeStartJobId{0};
    std::atomic<uint32_t> activeChunkJobId{0};
    std::atomic<StreamJobKind> jobInFlight{StreamJobKind::None};
};


class Request{
public:
    Request() {}
    Request(ResourceManager* resourceManager, std::pair<int,int> sampleIdentifier, StreamingBuffer* buffer, size_t requestId);

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
    size_t chunkStartIndex = 0;

    std::vector<size_t>* chunkStartIndices;
    size_t sampleLength;
    size_t chunkLength;
};
