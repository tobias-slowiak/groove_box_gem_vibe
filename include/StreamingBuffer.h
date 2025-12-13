#pragma once
//compile

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
#include "../include/SampleIdentifier.h"
#include "../include/DebugLog.h"
#include "../include/ResourceManager.h"
#include "../include/StreamingMessage.h"
#include "../include/TaskWrapper.h"
#include "../include/AudioStreamer.h"

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
            assert(parent != nullptr && "StreamingBufferIterator::operator* parent null");
            return ElementProxy(*this);
        }

        StreamingBufferIterator(StreamingBuffer* parent);

        StreamingBufferIterator(SampleIdentifier sampleIdentifier, size_t index, StreamingBuffer* parent, std::vector<int>* chunkIndicesInBuffer, SBIType type);

        StreamingBufferIterator& operator++();      // pre-increment

        StreamingBufferIterator operator++(int);    // post-increment

        void set(SampleIdentifier sampleIdentifier, size_t index, StreamingBuffer* parent, std::vector<int>* chunkIndicesInBuffer, SBIType type);

        void initialize();

        void release();

        SampleIdentifier sampleIdentifier;

    private:
        friend class StreamingBuffer;
        friend class AudioStreamer;
        friend class ElementProxy;

        size_t index;
        StreamingBuffer* parent;
        std::vector<int>* chunkIndicesInBuffer;
        SBIType type = SBIType::None;
        size_t sampleLength;
        int chunkIndex;
        size_t indexInChunk;
        int chunkIndexInBuffer;
        float* chunkStartPtr;
        float* data;
};

//TODO: when playing hard (many notes fast ) there is a problem that iterators get strange
//pointers to chunk, so when hitting a note it starts out with a couple of right chunks, but
//then it wildly jumps around in the chunks of multiple samples. so the assignment of chunks needs to
//be mrore strict

//I might get race conditions here. it would be better to have an array of mutex locks for the states.
struct ChunkState {
    std::atomic<int> ownerKey{-1};
    std::atomic<int> ownerVelocity{-1};
    std::atomic<int> chunkIndex{-1};
    std::atomic<bool> writeProtected{false};

    void set(SampleIdentifier sampleIdentifier, int chunkIndex, bool writeProtected);
};
static constexpr float END_OF_SAMPLE = std::numeric_limits<float>::lowest() + 2.0f;
static constexpr int CHUNK_INVALID = std::numeric_limits<int>::max() - 1;
static constexpr int ITERATOR_INVALID = std::numeric_limits<int>::max() - 2;
static constexpr int DEFAULT_STREAMING_CHUNK_SIZE = 8820;



class StreamingBuffer {
public:

    StreamingBuffer(ResourceManager* resourceManager,
            size_t bufferLengthInFrames,
            int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    ~StreamingBuffer();

    void setFolderPath(std::string path){folderPath = path;}

    StreamingBufferIterator& begin(SampleIdentifier sampleIdentifier, SBIType type);

    void sendStreamStartsMessages();

    void eraseIterator(StreamingBufferIterator& iterator);

    void audioCheckAndWorkMessages();

    void processBlockwise();

    //the two following methods are only for the bandwidth test
    bool streamerIsInFlight(){return audioStreamer.streamerIsInFlight();}
    
    void streamFullSample(SampleIdentifier sampleIdentifier);

    void flushToDisk(SampleIdentifier sampleIdentifier){audioStreamer.flushToDisk(sampleIdentifier);}

    std::string filename(SampleIdentifier sampleIdentifier){return folderPath + "/" + std::to_string(sampleIdentifier.first) + "_" + std::to_string(sampleIdentifier.second) + ".wav";}

    void protectSample(SampleIdentifier sampleIdentifier);

    void unProtectSample(SampleIdentifier sampleIdentifier);

    void sendInitializeSampleMessage(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames);

    void sendInitializeSamplesMessages(std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    void clear();

    void initializeForNewSamplePack(std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    int findFreeChunk();

    void stopWork();

    void mutate(StreamingMessage msg);

    void printInfo();

    void printIterators();

    void stream(StreamingMessage msg){audioStreamer.stream(msg);}

    void taskWorkMessage(std::string& taskName, StreamingMessage msg);

private:
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
    ChunkState* chunkStates = nullptr;
    std::vector<std::vector<float>> chunks;
    std::unordered_map<SampleIdentifier, std::vector<int>> chunkIndicesInBuffer; //synchronize with audioStreamer
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
