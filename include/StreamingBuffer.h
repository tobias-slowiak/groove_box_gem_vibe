#pragma once

#include <limits>
#include <libraries/sndfile/sndfile.h>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <cstdint>
#include <cassert>
#include <string>
#include <utility>

#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>
#include "../include/BasicUtilities.h"
#include "../include/SampleIdentifier.h"
#include "../include/DebugLog.h"
#include "../include/ResourceManager.h"
#include "../include/StreamingMessage.h"
#include "../include/TaskWrapper.h"
#include "../include/AudioStreamer.h"

static constexpr float END_OF_SAMPLE = std::numeric_limits<float>::lowest() + 2.0f;
static constexpr int CHUNK_INVALID = MAX_INT - 1, ITERATOR_INVALID = MAX_INT - 2, CHUNKSTATE_INVALID = MAX_INT - 3;
static constexpr int DEFAULT_STREAMING_CHUNK_SIZE = 8820;
static constexpr int DEFAULT_STREAMING_ADVANCE_IN_CHUNKS = 2;
//return codes for sampleInUse function
static constexpr int NOT_IN_USE = 0, IT_EXISTS = MAX_INT - 101, FLUSH_EXISTS = MAX_INT - 102, BOTH_EXIST = MAX_INT - 103;


class StreamingBuffer;
class StreamingBufferIterator;

enum class SBIType {
    None,
    Read,
    Write
};

class ElementProxy {
    public:
        ElementProxy(StreamingBufferIterator& iterator) : iterator(iterator) {}
        ElementProxy& operator=(float value);
        operator float() const;
    private:
        friend class StreamingBufferIterator;
        StreamingBufferIterator& iterator;
};

//TODO: make them stop if mutate task in flight?
class StreamingBufferIterator {
public:
    ElementProxy operator*(){
        return ElementProxy(*this);
    }

    StreamingBufferIterator& operator++();      // pre-increment

    StreamingBufferIterator operator++(int);    // post-increment

    void release();

    SampleIdentifier sampleIdentifier;

private:
    StreamingBufferIterator(StreamingBuffer& parent);

    void set(SampleIdentifier sampleIdentifier,
        std::vector<int>* chunkIndicesInBuffer,
        SBIType type,
        int streamingAdvanceInBlock = DEFAULT_STREAMING_ADVANCE_IN_CHUNKS);

    void initialize();

    friend class StreamingBuffer;
    friend class AudioStreamer;
    friend class ElementProxy;

    size_t index;
    StreamingBuffer& parent;
    std::vector<int>* chunkIndicesInBuffer;
    SBIType type = SBIType::None;
    int streamingAdvanceInChunks;
    size_t sampleLength;
    int chunkIndex;
    size_t indexInChunk;
    int chunkIndexInBuffer;
    float* chunkStartPtr;
    float* data;
};

//TODO: only the bools need to be atomic, everything else is audio thread only
struct ChunkState {
    std::atomic<int> ownerKey{CHUNKSTATE_INVALID};
    std::atomic<int> ownerVelocity{CHUNKSTATE_INVALID};
    std::atomic<int> chunkIndex{CHUNKSTATE_INVALID};
    std::atomic<bool> writeProtected{false};
    std::atomic<bool> chunkReady{false};

    void set(SampleIdentifier sampleIdentifier, int chunkIndex, bool writeProtected, bool inChunkReady);
};

class StreamingBuffer {
public:

    StreamingBuffer(ResourceManager* resourceManager,
            size_t bufferLengthInFrames,
            int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    ~StreamingBuffer();

    void setFolderPath(std::string path){folderPath = path;}

    StreamingBufferIterator& begin(SampleIdentifier sampleIdentifier, SBIType type, float playbackRate = 1.0);

    //TODO for the future: maybe let this be and only checkandworkmessages when sent.
    void processBlockwise();

    //the two following methods are only for the bandwidth test
    bool streamerIsInFlight(){return audioStreamer.streamerIsInFlight();}
    
    void streamFullSample(SampleIdentifier sampleIdentifier);

    void printInfo();

    void printIterators();
    
    void taskWorkMessage(std::string& taskName, StreamingMessage msg);

private:

    int sampleInUse(SampleIdentifier sampleIdentifier);

    void sendStreamStartsMessages();

    void releaseIterator(StreamingBufferIterator& iterator);

    std::string filename(SampleIdentifier sampleIdentifier){return folderPath + "/" + std::to_string(sampleIdentifier.first) + "_" + std::to_string(sampleIdentifier.second) + ".wav";}

    void protectSample(SampleIdentifier sampleIdentifier);

    void unProtectSample(SampleIdentifier sampleIdentifier);

    void sendInitializeSampleMessage(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames);

    void sendInitializeSamplesMessages(std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    void clear();

    void initializeForNewSamplePack(std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    int findFreeChunk();

    int assignToFreeChunk(SampleIdentifier sampleIdentifier, int chunkIndex, std::vector<int>& chunkIndicesInBuffer);

    size_t getSampleLength(SampleIdentifier sampleIdentifier);

    std::vector<int>& getChunkIndicesInBuffer(SampleIdentifier sampleIdentifier);

    void stopWork();

    void workMutateMessage(StreamingMessage msg);



    friend class AudioStreamer;
    friend class StreamingBufferIterator;
    friend class SamplePack;


    ResourceManager* resourceManager;
    std::string bufferName;
    std::string folderPath;
    //chunk storage
    int chunkLength = DEFAULT_STREAMING_CHUNK_SIZE;

    int totalNumberOfChunks;
    int totalNumberOfIterators;
    std::unordered_map<SampleIdentifier, size_t>& availableSamples;
    std::vector<StreamingBufferIterator> iterators;
    AudioStreamer audioStreamer;
    //TODO: make this a vector?
    ChunkState* chunkStates = nullptr;
    std::vector<std::vector<float>> chunks;
    std::unordered_map<SampleIdentifier, std::vector<int>> chunkIndicesInBufferMap; //synchronize with audioStreamer
    size_t StreamingBufferIteratorAssignIndex = 0;

    std::atomic<size_t> freeChunkSearchIdx{0};


    //protection of chunks (regarding chunkstates upstairs)
    /*
    TODO: check if this is lock free:
        struct Slot { std::pair<int,int> a; int b; };
        static_assert(std::is_trivially_copyable_v<Slot>);
        std::atomic<Slot> slot;

        static_assert(std::atomic<Slot>::is_always_lock_free);
    

    The following is more or less a reader lock.
    There is now writer lock and it would be more thread safe if there was one
    however no reader should read from a chunk that is currently written to since
    writers (either stream thread or audio thread via write iterator)
    only write to invalid chunks and if a reader reads from such a chunk that is
    written to i know that there is a mistake in the management of
    the chunkIndicesInBuffer container
    */
    //Auxiliary Tasks
    std::atomic<bool> mutateOngoing{false};
    int mutateDataPrio = 70;
    std::string mutateDataTaskName;
    TaskWrapper<StreamingBuffer, StreamingMessage> mutateDataTask;
};

