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
#include "../include/DebugLog.h"
#include "../include/ResourceManager.h"

class StreamingBuffer;
class StreamingBufferIterator;

//AI Generated hash - check if this works.
// Hash for SampleIdentifier (pair<int,int>) tailored to expected 0–256 range
using SampleIdentifier = std::pair<int, int>;

namespace std {
template <>
struct hash<SampleIdentifier> {
    size_t operator()(const SampleIdentifier& s) const noexcept {
        return (static_cast<size_t>(s.first & 0x1ff) << 9) ^ static_cast<size_t>(s.second & 0x1ff);
    }
};
} // namespace std


//I might get race conditions here. it would be better to have an array of mutex locks for the states.
struct ChunkState {
    std::atomic<int> ownerKey{-1};
    std::atomic<int> ownerVelocity{-1};
    std::atomic<int> chunkIndex{-1};
    std::atomic<bool> writeProtected{false};

    void set(SampleIdentifier sampleIdentifier, int chunkIndex, bool writeProtected);
};
static constexpr float END_OF_SAMPLE = std::numeric_limits<float>::lowest() + 2.0f;
static constexpr float SAMPLE_NOT_LOADED_VALUE = std::numeric_limits<float>::lowest() + 1.0f;
static constexpr int CHUNK_INVALID = std::numeric_limits<int>::max() - 1;
static constexpr int ITERATOR_INVALID = std::numeric_limits<int>::max() - 2;

enum class StreamingMessageType : uint8_t { ChunkReady,
    InvalidatedChunk,
    StreamChunk, 
    AssignChunk, 
    InitializeSample,
    Clear, 
    FlushInfo, 
    FlushChunk, 
    FlushComplete
};

struct StreamingMessage {
    StreamingMessageType type;
    SampleIdentifier sampleIdentifier;
    int chunkIndex;
    int chunkIndexInBuffer;
};

class StreamingMessageQueue {
public:

    StreamingMessageQueue() {
        assert(messages.size() > 0  && "caught default smq ctor which should never trigger");}
    
    StreamingMessageQueue(size_t capacity): capacity(capacity){
        assert(capacity > 0 && "why get 0 capacity on messagequeue?");
        messages.resize(capacity);
        assert(messages.size() > 0 && "what smq fuck?");}
    
    bool push(StreamingMessage value);

    bool pop(StreamingMessage& out);

private:
    size_t capacity;
    std::vector<StreamingMessage> messages;
    std::atomic<size_t> tail{0};
    std::atomic<size_t> head{0}; // producer-only
};

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
        ElementProxy operator*(){return ElementProxy(*this);}

        StreamingBufferIterator(StreamingBuffer* parent);

        StreamingBufferIterator(SampleIdentifier sampleIdentifier, size_t index, StreamingBuffer* parent, std::vector<int>* chunkIndicesInBuffer, SBIType type);

        StreamingBufferIterator& operator++();      // pre-increment

        StreamingBufferIterator operator++(int);    // post-increment

        void set(SampleIdentifier sampleIdentifier, size_t index, StreamingBuffer* parent, std::vector<int>* chunkIndicesInBuffer, SBIType type);

        void initialize();

        SampleIdentifier sampleIdentifier;

    private:
        friend class StreamingBuffer;
        friend class AudioStreamer;
        friend class ElementProxy;

        size_t index;
        StreamingBuffer* parent;
        std::vector<int>* chunkIndicesInBuffer;
        SBIType type = SBIType::None;
        int chunkIndex;
        size_t indexInChunk;
        int chunkIndexInBuffer;
        float* chunkStartPtr;
        float* data;
};


void streamOnThread(void* arg);
void flushOnThread(void* arg);
void mutateOnThread(void* arg);

class AudioStreamer{
public:
    //Audio thread only 
    AudioStreamer(StreamingBuffer& streamingBuffer);

    void streamChunk(SampleIdentifier sampleIdentifier, int chunkIndex, int chunkIndexInBuffer);

    void streamFromDisk(SampleIdentifier sampleIdentifier);

    void streamStartFromDisk(SampleIdentifier sampleIdentifier);

    void flushToDisk(SampleIdentifier sampleIdentifier);

    void processBlockwise();

    //Stream Thread only

    void stream();

    //Flush Thread only
    void flush();

private:
    enum class ScheduleStatus {
        Scheduled,
        Busy,
        Error
    };
    friend class StreamingBuffer;
    friend class StreamingBufferIterator;
    StreamingBuffer& parent;
    
    //thread schedule flags
    bool streamNeedsScheduling = false;
    bool flushNeedsScheduling = false;

    //Auxiliary Tasks
    AuxiliaryTask streamSamplesTask;
    AuxiliaryTask flushToDiskTask;
    int streamSamplePrio = 50;
    int flushToDiskPrio = 20;
    std::string streamSamplesTaskName;
    std::string flushToDiskTaskName;

    //Messaging between threads
    static constexpr size_t queueCapacity = 512;
    StreamingMessageQueue audioToStream{queueCapacity};
    StreamingMessageQueue audioToFlush{queueCapacity};
    StreamingMessageQueue streamToAudio{queueCapacity};
    StreamingMessageQueue flushToAudio{queueCapacity};

    //Stream Thread Only
    std::unordered_map<SampleIdentifier, std::vector<int>> s_chunkIndicesInBuffer; //synchronize with parent
    std::vector<float> streamBuffer;

    std::unordered_set<SampleIdentifier> pendingFlushes;
    //Flush Thread Only
    std::unordered_map<SampleIdentifier, int> f_numberOfFlushableChunks;
    std::vector<float> flushBuffer;

    //Job/Task management
    std::atomic<bool> streamingTaskInFlight{false};
    std::atomic<bool> flushTaskInFlight{false};
};

class StreamingBuffer {
public:

    StreamingBuffer(ResourceManager* resourceManager,
            size_t bufferLengthInFrames,
            int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    ~StreamingBuffer();

    StreamingBufferIterator& begin(SampleIdentifier sampleIdentifier, SBIType type);

    void streamStarts();

    void eraseIterator(StreamingBufferIterator& iterator);

    void processBlockwise();

    void streamFromDisk(SampleIdentifier sampleIdentifier){audioStreamer.streamFromDisk(sampleIdentifier);}

    void flushToDisk(SampleIdentifier sampleIdentifier){audioStreamer.flushToDisk(sampleIdentifier);}

    std::string filename(SampleIdentifier sampleIdentifier){return folderPath + "/" + std::to_string(sampleIdentifier.first) + "_" + std::to_string(sampleIdentifier.second) + ".wav";}

    void protectSample(SampleIdentifier sampleIdentifier);

    void unProtectSample(SampleIdentifier sampleIdentifier);

    void initializeSample(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames);

    void initializeSamples(std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    int findFreeChunk();

    void mutate();

    void stream(){audioStreamer.stream();}

    void clear();

private:
    friend class AudioStreamer;
    friend class StreamingBufferIterator;


    ResourceManager* resourceManager;
    std::string bufferName;
    std::string folderPath;
    //chunk storage
    int chunkLength = 8820;

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
    int mutateDataPrio = 70;
    std::string mutateDataTaskName;
    AuxiliaryTask MutateDataTask;

    bool mutateDataTaskNeedsScheduling = false;

    //Messaging between threads
    static constexpr size_t mutateQueueCapacity = 512;
    StreamingMessageQueue audioToMutate{mutateQueueCapacity};
    StreamingMessageQueue mutateToAudio{mutateQueueCapacity};

    //Mutate Thread Only
    //buffers if needed

    //Job/Task management
    std::atomic<bool> mutateTaskInFlight{false};
};
