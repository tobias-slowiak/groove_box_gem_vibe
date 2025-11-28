#include <limits>
#include <sndfile.h>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <cstdint>
#include <cassert>
#include <utility>

#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>
#include "../include/DebugLog.h"
#include "../include/ResourceManager.h"

//////////////////////////HEADER FILE

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

struct StreamingMessage {
    enum class StreamingMessageType : uint8_t { ChunkReady,
        InvalidatedChunk,
        StreamChunk, 
        AssignChunk, 
        InitializeSample,
        Clear, 
        FlushInfo, 
        FlushChunk, 
        FlushComplete};
    StreamingMessageType type;
    SampleIdentifier sampleIdentifier;
    int chunkIndex;
    int chunkIndexInBuffer;
};

class StreamingMessageQueue {
public:
    StreamingMessageQueue(size_t capacity): capacity(capacity){messages.resize(capacity);}
    
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

        StreamingBufferIterator(StreamingBuffer& parent);

        StreamingBufferIterator(SampleIdentifier sampleIdentifier, size_t index, StreamingBuffer& parent, std::vector<int>& chunkIndicesInBuffer, SBIType type);

        StreamingBufferIterator& operator++();      // pre-increment

        StreamingBufferIterator operator++(int);    // post-increment


    private:
        friend class StreamingBuffer;
        StreamingBuffer& parent;
        SBIType type = SBIType::None;
        SampleIdentifier sampleIdentifier;
        std::vector<int>& chunkIndicesInBuffer;
        float* chunkStartPtr;
        float* data;
        size_t index;
        int chunkIndex;
        int chunkIndexInBuffer;
        size_t indexInChunk;
};

class WavWriter {
public:

    WavWriter(std::string path, ResourceManager* resourceManager, sf_count_t totalFrames);

    void writeChunk(sf_count_t frameOffset, const std::vector<float>& interleaved);

    ~WavWriter() { if(f) sf_close(f); }

private:
    SNDFILE* f = nullptr;
    SF_INFO info{};
    sf_count_t frames = 0;
};

//////////////////Thread wrappers
void streamOnThread(void* arg);
void flushOnThread(void* arg);
void mutateOnThread(void* arg);

class AudioStreamer{
public:
    //Audio thread only 
    AudioStreamer(StreamingBuffer& streamingBuffer);

    void streamFromDisk(SampleIdentifier sampleIdentifier);

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
    static constexpr size_t queueCapacity = 256;
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
            int totalNumberOfChunks, int chunkLength, int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    ~StreamingBuffer();

    StreamingBufferIterator& begin(SampleIdentifier sampleIdentifier, SBIType type);

    void eraseIterator(StreamingBufferIterator& iterator);

    void processBlockwise();

    void streamFromDisk(SampleIdentifier sampleIdentifier){audioStreamer.streamFromDisk(sampleIdentifier);}

    void flushToDisk(SampleIdentifier sampleIdentifier){audioStreamer.flushToDisk(sampleIdentifier);}

    std::string filename(SampleIdentifier sampleIdentifier){return folderPath + "/" + sampleIdentifier.first + "_" + sampleIdentifier.second + ".wav";}

    void protectSample(SampleIdentifier sampleIdentifier);

    void unProtectSample(SampleIdentifier sampleIdentifier);

    void initializeSample(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames);

    void initializeSamples(std::unordered_map<SampleIdentifier, size_t>& availableSamples);

    int findFreeChunk();

    void mutate();

    void clear();

private:
    friend class AudioStreamer;

    ResourceManager* resourceManager;
    //chunk storage
    std::vector<std::vector<float>> chunks;
    int chunkLength;
    int defaultSampleLengthInChunks = 150;

    AudioStreamer audioStreamer;

    std::string bufferName;
    std::string folderPath;

    int totalNumberOfChunks;
    std::unordered_map<SampleIdentifier, size_t>& availableSamples;
    std::unordered_map<SampleIdentifier, std::vector<int>> chunkIndicesInBuffer; //synchronize with audioStreamer
    int totalNumberOfIterators;
    std::vector<StreamingBufferIterator> iterators;
    size_t StreamingBufferIteratorAssignIndex = 0;

    std::atomic<size_t> freeChunkSearchIdx{0};
    //protection of chunks
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
    ChunkState* chunkStates = nullptr;

    //Auxiliary Tasks
    std::string mutateDataTaskName;
    AuxiliaryTask MutateDataTask;
    int mutateDataPrio = 70;

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

////////////////////////CPP FILE

///////////ChunkState/////////

void ChunkState::set(SampleIdentifier sampleIdentifier, int inChunkIndex, bool inWriteProtected){
    ownerKey.store(sampleIdentifier.first, std::memory_order_release);
    ownerVelocity.store(sampleIdentifier.second, std::memory_order_release);
    chunkIndex.store(inChunkIndex, std::memory_order_release);
    writeProtected.store(inWriteProtected, std::memory_order_release);
}


///////////StreamingMessageQueue///////////
bool StreamingMessageQueue::push(StreamingMessage value) {
    size_t currentHead = head.load(std::memory_order_relaxed);
    size_t nextHead = (currentHead + 1) % capacity;
    if(nextHead == tail.load(std::memory_order_acquire)){
        throw std::runtime_error("StreamingMessageQueue::push queue full, investigate!");
        return false;
    }
    assert(messages.size() > currentHead);
    messages.at(currentHead) = value;
    head.store(nextHead, std::memory_order_release);
    return true;
}

bool StreamingMessageQueue::pop(StreamingMessage& out) {
    size_t currentTail = tail.load(std::memory_order_relaxed);
    size_t currentHead = head.load(std::memory_order_acquire);
    if(currentTail == currentHead)
        return false;
    assert(messages.size() > currentTail);
    out = messages.at(currentTail);
    tail.store((currentTail + 1) % capacity, std::memory_order_release);
    return true;
}

///////////ElementProxy///////////

ElementProxy& ElementProxy::operator=(float value){
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        DEBUG_RT_PRINTF("ElementProxy::operator=: WARNING: trying to write to invalid iterator\n");
        return *this;
    }
    if(value == END_OF_SAMPLE){
        if(*(iterator.data) != END_OF_SAMPLE){
            throw std::runtime_error("ElementProxy::operator= write iterator tries to change length of sample -> initializeSample before!\n");
        }
        iterator.parent.eraseIterator(iterator);
    }
    *(iterator.data) = value;
    return *this;
}


float ElementProxy::operator float() const {
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        return END_OF_SAMPLE;
    }
    if(*(iterator.data) == END_OF_SAMPLE){
        iterator.parent.eraseIterator(iterator);
    }
    return *(iterator.data);
}

///////////StreamingBufferIterator///////////

namespace {
    std::vector<int> EMPTY_CHUNK_INDEX_VECTOR = std::vector<int>();
}
StreamingBufferIterator::StreamingBufferIterator(StreamingBuffer& parent)
        : sampleIdentifier({ITERATOR_INVALID,ITERATOR_INVALID}),
        index(0),
        parent(parent),
        chunkIndicesInBuffer(EMPTY_CHUNK_INDEX_VECTOR),
        type(SBIType::None),
        chunkIndex(ITERATOR_INVALID),
        indexInChunk(ITERATOR_INVALID),
        chunkIndexInBuffer(ITERATOR_INVALID),
        chunkStartPtr(nullptr),
        data(nullptr){}

StreamingBufferIterator::StreamingBufferIterator(SampleIdentifier sampleIdentifier, size_t index,
                                            StreamingBuffer& parent, std::vector<int>& chunkIndicesInBuffer,
                                            SBIType type)
        : sampleIdentifier(sampleIdentifier),
        index(index),
        parent(parent),
        chunkIndicesInBuffer(chunkIndicesInBuffer),
        type(type),
        chunkIndex(index / parent.chunkLength),
        indexInChunk(index % parent.chunkLength),
        chunkIndexInBuffer(chunkIndicesInBuffer.at(chunkIndex)),
        chunkStartPtr(parent.chunks.at(chunkIndexInBuffer).data()),
        data(chunkStartPtr + indexInChunk) {}

StreamingBufferIterator StreamingBufferIterator::operator++(int){    // post-increment
    StreamingBufferIterator tmp = *this;
    ++(*this);
    return tmp;
}

StreamingBufferIterator& StreamingBufferIterator::operator++(){      // pre-increment
    if(sampleIdentifier.first == ITERATOR_INVALID){
        return *this;
    }
    ++indexInChunk;
    if(indexInChunk >= parent.chunkLength){
        ++chunkIndex;
        indexInChunk = 0;
        if(chunkIndicesInBuffer.size() <= chunkIndex){
            throw std::runtime_error("StreamingBufferIterator::operator++: iterator incremented past end of sample\n");
        } else {
            chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
            if(chunkIndexInBuffer == CHUNK_INVALID){
                if(type == SBIType::Write){
                    chunkIndexInBuffer = parent.findFreeChunk();
                    chunkIndicesInBuffer.at(chunkIndex) = chunkIndexInBuffer;
                    parent.audioStreamer.audioToStream.push({StreamingMessageType::AssignChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer});
                    parent.audioStreamer.streamNeedsScheduling = true;
                    parent.chunkStates[chunkIndexInBuffer].set(sampleIdentifier, chunkIndex, true);
                }
                if(type == SBIType::Read){
                    throw std::runtime_error("StreamingBufferIterator::operator++: stream too slow");
                }
            }
            chunkStartPtr = parent.chunks.at(chunkIndexInBuffer).data();
            data = chunkStartPtr;
        }
    } else {
        ++data;
    }
    return *this;
}

///////////StreamingBuffer///////////

StreamingBuffer::StreamingBuffer(ResourceManager* resourceManager,
            int totalNumberOfChunks, int chunkLength,
            int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples)
            : resourceManager(resourceManager), chunkLength(chunkLength),
            bufferName(bufferName), folderPath(folderPath),
            totalNumberOfChunks(totalNumberOfChunks), totalNumberOfIterators(totalNumberOfIterators),
            availableSamples(availableSamples),
            iterators(std::vector<StreamingBufferIterator>(totalNumberOfIterators, StreamingBufferIterator(*this))),
            audioStreamer(*this),
            chunkStates(new ChunkState[totalNumberOfChunks]),
            chunks(totalNumberOfChunks, std::vector<float>(chunkLength)),
            mutateDataTaskName(bufferName + "_MTask"),
            MutateDataTask(Bela_createAuxiliaryTask(mutateOnThread, mutateDataPrio, mutateDataTaskName.c_str(), (void*)this))
{
    //TODO: make a version of this function that runs on the audiothread at setup time.
    initializeSamples(availableSamples);
}

StreamingBuffer::StreamingBuffer(ResourceManager* resourceManager,
            size_t bufferLengthInFrames,
            int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    size_t defaultBufferLength = static_cast<size_t>(resourceManager->audioFramesPerSecond * 0.2);
    chunkLength = defaultBufferLength;
    totalNumberOfChunks = static_cast<int>(bufferLengthInFrames / chunkLength) + 1;
    *this = StreamingBuffer(resourceManager, totalNumberOfChunks, chunkLength,
        totalNumberOfIterators, bufferName, folderPath, availableSamples);
}

StreamingBuffer::~StreamingBuffer(){
    delete[] chunkStates;
}

void mutateOnThread(void* arg){
    DEBUG_PRINTF("\n\mutateOnThread\n\n");
    StreamingBuffer* streamingBuffer = static_cast<StreamingBuffer*>(arg);
    streamingBuffer->mutate();
}

void StreamingBuffer::mutate(){
    StreamingMessage msg;
    while(audioToMutate.pop(msg)){
        if(msg.type == StreamingMessageType::InitializeSample){
            auto [it, inserted] = chunkIndicesInBuffer.try_emplace(msg.sampleIdentifier);
            std::vector<int>& localChunkIndicesInBuffer = it->second;
            if(!inserted){
                for(int chunkIndexInBuffer: localChunkIndicesInBuffer){
                    if(chunkIndexInBuffer != CHUNK_INVALID){
                        chunkStates[chunkIndexInBuffer].set({-1,-1}, -1, false);
                    }
                }
            }
            chunkIndicesInBuffer[msg.sampleIdentifier] = std::vector<int>(msg.chunkIndex, CHUNK_INVALID);
            audioStreamer.s_chunkIndicesInBuffer[msg.sampleIdentifier] = std::vector<int>(msg.chunkIndex, CHUNK_INVALID);
        }
        if(msg.type == StreamingMessageType::Clear){
            chunkIndicesInBuffer.clear();
            audioStreamer.s_chunkIndicesInBuffer.clear();
            audioStreamer.pendingFlushes.clear();
            audioStreamer.f_numberOfFlushableChunks.clear();
            for(StreamingBufferIterator& it : iterators){
                it = StreamingBufferIterator(*this);
            }
            for(size_t chunkStateIndex = 0; chunkStateIndex < totalNumberOfChunks; chunkStateIndex++){
                chunkStates[chunkStateIndex].set({-1,-1}, -1, false);
            }
            StreamingMessage dummyMsg;
            while(audioStreamer.audioToStream.pop(dummyMsg)){}
            while(audioStreamer.flushToAudio.pop(dummyMsg)){}
            while(audioStreamer.streamToAudio.pop(dummyMsg)){}
            while(audioStreamer.audioToFlush.pop(dummyMsg)){}
        }
    }
    mutateTaskInFlight.store(false, std::memory_order_release);
}

void StreamingBuffer::clear(){
    audioToMutate.push({StreamingMessageType::Clear, {-1,-1}, -1, -1});
    mutateDataTaskNeedsScheduling = true;
}

StreamingBufferIterator& StreamingBuffer::begin(SampleIdentifier sampleIdentifier, SBIType type){
    auto [it, inserted] = chunkIndicesInBuffer.try_emplace(sampleIdentifier);
    std::vector<int>& localChunkIndicesInBuffer = it->second;
    for(int i = 0; i < totalNumberOfIterators; i++){
        StreamingBufferIteratorAssignIndex = (StreamingBufferIteratorAssignIndex + 1) % totalNumberOfIterators;
        if(iterators.at(StreamingBufferIteratorAssignIndex).sampleIdentifier.first == ITERATOR_INVALID){
            break;
        }
        if(i == totalNumberOfIterators - 1){
            throw std::runtime_error("StreamingBuffer::begin: no free iterator slots available. This should never happen. Probably there is a bug that iterators dont get invalidated that should\n");
        }
    }
    if(inserted){
        throw std::runtime_error("StreamingBuffer::begin: sample not initialized. Call initializeSample before begin. maybe implement this in the future.\n");
    }
    iterators.at(StreamingBufferIteratorAssignIndex) = StreamingBufferIterator(sampleIdentifier, 0, *this, localChunkIndicesInBuffer, type);
    assert(localChunkIndicesInBuffer.size() > 0 && "StreamingBuffer::begin: chunkIndicesInBuffer not initialized for sampleIdentifier");
    if (localChunkIndicesInBuffer.at(0) == CHUNK_INVALID) {
        int chunkIndexInBuffer = findFreeChunk();
        localChunkIndicesInBuffer.at(0) = chunkIndexInBuffer;
        audioStreamer.audioToStream.push({StreamingMessageType::AssignChunk, sampleIdentifier, 0, chunkIndexInBuffer});
        audioStreamer.streamNeedsScheduling = true;
        if(type == SBIType::Read){
            chunks.at(chunkIndexInBuffer).at(0) = SAMPLE_NOT_LOADED_VALUE;
            audioStreamer.streamFromDisk(sampleIdentifier);
        }
        if(type == SBIType::Write){  
            chunkStates[chunkIndexInBuffer].set(sampleIdentifier, 0, true);
        }
    } else {
        protectSample(sampleIdentifier);
        if(type == SBIType::Read){
            audioStreamer.streamFromDisk(sampleIdentifier);
        }
    }
    return iterators.at(StreamingBufferIteratorAssignIndex);
}

void StreamingBuffer::eraseIterator(StreamingBufferIterator& iterator){
    auto it = std::find_if(iterators.begin(), iterators.end(),
        [&iterator](const StreamingBufferIterator& elem) {
            return &elem == &iterator;
        }); //check if adress of elem is equal to address of iterator
    if (it != iterators.end()) {
        StreamingBufferIterator& itRef = *it;
        if(itRef.type == SBIType::Write){
            flushToDisk(itRef.sampleIdentifier);
        }
        SampleIdentifier oldSampleIdentifier = itRef.sampleIdentifier;
        itRef = StreamingBufferIterator(*this);
        bool exists = false;
        //check if I still have to protect the sample

        //is there another iterator?
        exists = std::any_of(iterators.begin(),iterators.end(),
            [&](const StreamingBufferIterator& iter) {
                return iter.sampleIdentifier == oldSampleIdentifier;
            }
        );
        if(exists) return;

        //Is there a flush going for the sample?
        exists = std::any_of(audioStreamer.pendingFlushes.begin(),audioStreamer.pendingFlushes.end(),
            [&](const SampleIdentifier& sId) {
                return sId == oldSampleIdentifier;
            }
        );
        if(exists) return;

        unProtectSample(oldSampleIdentifier);
    } else {
        throw std::runtime_error("StreamingBuffer::eraseIterator: iterator not found in iterators vector\n");
    }
}

void StreamingBuffer::processBlockwise(){
    if(mutateDataTaskNeedsScheduling){
        bool expected = false;
        if(mutateTaskInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel)){
            int scheduleResponse = Bela_scheduleAuxiliaryTask(MutateDataTask);
            if(scheduleResponse == 0){
                mutateDataTaskNeedsScheduling = false;
            } else {
                mutateTaskInFlight.store(false, std::memory_order_release);
                if(scheduleResponse != EBUSY){
                    DEBUG_RT_PRINTF("streamingbuffer::processBlockwise(): Bela_scheduleAuxiliaryTask sent error code: %d\n", scheduleResponse);
                }
            }
        }
    }
    //TODO: barrier if mutate task in flight?
    audioStreamer.processBlockwise();
}

void StreamingBuffer::protectSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& localChunkIndicesInBuffer = chunkIndicesInBuffer[sampleIdentifier];
    for(int chunkIndex = 0; chunkIndex < localChunkIndicesInBuffer.size(); chunkIndex++){
        assert(localChunkIndicesInBuffer.size() > chunkIndex && "protectsample: chunkindicesinbuffer to small");
        if(localChunkIndicesInBuffer.at(chunkIndex) == CHUNK_INVALID){
            continue;
        }
        int chunkIndexInBuffer = localChunkIndicesInBuffer.at(chunkIndex);
        chunkStates[chunkIndexInBuffer].writeProtected.store(true, std::memory_order_release);
    }
}

void StreamingBuffer::unProtectSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& localChunkIndicesInBuffer = chunkIndicesInBuffer[sampleIdentifier];
    for(int chunkIndex = 0; chunkIndex < localChunkIndicesInBuffer.size(); chunkIndex++){
        assert(localChunkIndicesInBuffer.size() > chunkIndex && "unprotectsample: chunkindicesinbuffer to small");
        if(localChunkIndicesInBuffer.at(chunkIndex) == CHUNK_INVALID){
            continue;
        }
        int chunkIndexInBuffer = localChunkIndicesInBuffer.at(chunkIndex);
        chunkStates[chunkIndexInBuffer].writeProtected.store(false, std::memory_order_release);
    }
}

void StreamingBuffer::initializeSamples(std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    clear();
    for(auto& sampleInfo: availableSamples){
        initializeSample(sampleInfo.first, sampleInfo.second);
    }
}

void StreamingBuffer::initializeSample(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames){
    int numberOfChunks = static_cast<int>(expectedSampleLengthInFrames / chunkLength) + 1;
    audioToMutate.push({StreamingMessageType::InitializeSample, sampleIdentifier, numberOfChunks, -1});
    mutateDataTaskNeedsScheduling = true;
}

int StreamingBuffer::findFreeChunk(){
    const size_t chunkCount = chunks.size();
    if(chunkCount == 0){
        throw std::runtime_error("findFreeChunk called with no chunks allocated\n");
    }

    // Start from a rotating index to avoid scanning from 0 each time.
    size_t start = freeChunkSearchIdx.fetch_add(1, std::memory_order_acq_rel) % chunkCount;
    for(size_t offset = 0; offset < chunkCount; ++offset){
        size_t chunkIndexInBuffer = (start + offset) % chunkCount;
        int chunkIndex = chunkStates[chunkIndexInBuffer].chunkIndex.load(std::memory_order_acquire);
        if(chunkIndex != 0){ //NEVER OVERWRITE A FIRST CHUNK
            if(!( chunkStates[chunkIndexInBuffer].writeProtected.load(std::memory_order_acquire) )){
                return static_cast<int>(chunkIndexInBuffer);
            }
        }
    }
    throw std::runtime_error("did not find a free chunk\n");
    return -1;
}

///////////WavWriter///////////

WavWriter::WavWriter(std::string filename, ResourceManager* resourceManager, sf_count_t totalFrames) {
    info.samplerate = resourceManager->audioFramesPerSecond;
    //TODO: stereo will fail among other reasons bc. the flush and stream buffers are made for mono, should then be interleaved.
    info.channels   = resourceManager->inMonoMode ? 1 : 2;
    info.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT; // 32‑bit float WAV
    frames = totalFrames;

    f = sf_open(filename.c_str(), SFM_WRITE, &info);
    if(!f) throw std::runtime_error(sf_strerror(nullptr));

    // Pre-size the file to known length
    if(sf_command(f, SFC_FILE_TRUNCATE, &frames, sizeof(frames)) != SF_TRUE) {
        // Fallback: write zeros
        //TODO: wouldnt it be better to directly write the correct values on the first go?
        std::vector<float> zeros(4096 * info.channels, 0.f);
        for(sf_count_t position = 0; position < frames; ) {
            sf_count_t batchLength = std::min<sf_count_t>(frames - position, zeros.size() / info.channels);
            sf_writef_float(f, zeros.data(), batchLength);//the write pointer automagically advances
            position += batchLength;
        }
    }
    sf_seek(f, 0, SEEK_SET);
}

// write interleaved chunk starting at frameOffset
void WavWriter::writeChunk(sf_count_t frameOffset, const std::vector<float>& interleaved) {
    sf_count_t framesToWrite = interleaved.size() / info.channels;
    if(frameOffset + framesToWrite > frames) throw std::out_of_range("chunk beyond file length");
    if(sf_seek(f, frameOffset, SEEK_SET) < 0) throw std::runtime_error("seek failed");
    sf_count_t written = sf_writef_float(f, interleaved.data(), framesToWrite);
    if(written != framesToWrite) throw std::runtime_error("partial write");
}

///////////AudioStreamer///////////

AudioStreamer::AudioStreamer(StreamingBuffer& streamingBuffer): parent(streamingBuffer),
        streamBuffer(std::vector<float>(parent.chunkLength)),
        flushBuffer(std::vector<float>(parent.chunkLength)){
    streamSamplesTaskName = parent.bufferName + "_STask";
    flushToDiskTaskName = parent.bufferName + "_FTask";
    streamSamplesTask = Bela_createAuxiliaryTask(streamOnThread, streamSamplePrio, streamSamplesTaskName.c_str(), (void*)this);
    flushToDiskTask = Bela_createAuxiliaryTask(flushOnThread, flushToDiskPrio, flushToDiskTaskName.c_str(), (void*)this);
}

void AudioStreamer::streamFromDisk(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = parent.chunkIndicesInBuffer[sampleIdentifier];
    int numberOfChunks = chunkIndicesInBuffer.size();
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        if(chunkIndexInBuffer == CHUNK_INVALID){
            audioToStream.push({StreamingMessageType::StreamChunk, sampleIdentifier, chunkIndex, -1});
            streamNeedsScheduling = true;
        } else if(parent.chunks.at(chunkIndexInBuffer).at(0) == SAMPLE_NOT_LOADED_VALUE){
            audioToStream.push({StreamingMessageType::StreamChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer});
            streamNeedsScheduling = true;
        }
    }

}

void streamOnThread(void* arg){
    DEBUG_PRINTF("\n\streamOnThread\n\n");
    AudioStreamer* audioStreamer = static_cast<AudioStreamer*>(arg);
    audioStreamer->stream();
}


void AudioStreamer::stream(){ //stream thread only
    StreamingMessage msg;
    while(audioToStream.pop(msg)){
        std::vector<int>& chunkIndicesInBuffer = s_chunkIndicesInBuffer[msg.sampleIdentifier];
        int chunkIndexInBuffer = msg.chunkIndexInBuffer;
        int chunkIndex = msg.chunkIndex;
        if(msg.type == StreamingMessageType::StreamChunk){
            if(chunkIndexInBuffer == -1){
                chunkIndexInBuffer = parent.findFreeChunk();
                ChunkState& chunkState = parent.chunkStates[chunkIndexInBuffer];
                SampleIdentifier oldSampleIdentifier = {chunkState.ownerKey.load(std::memory_order_acquire), 
                    chunkState.ownerVelocity.load(std::memory_order_acquire)};
                int oldChunkIndex = chunkState.chunkIndex.load(std::memory_order_acquire);
                chunkState.set(msg.sampleIdentifier, chunkIndex, true);
                if(oldSampleIdentifier.first != -1){
                    std::vector<int>& oldChunkIndicesInBuffer = s_chunkIndicesInBuffer[oldSampleIdentifier];
                    assert(oldChunkIndicesInBuffer.size() > oldChunkIndex && "AudioStreamer::stream: oldChunkIndex out of range. This shuould never happen.There is a bug that s_chunkIndicesinbuffer is not synchronized right.");
                    oldChunkIndicesInBuffer.at(oldChunkIndex) = CHUNK_INVALID;
                }
            }

            int channel = 0; //maybe make stereo possible at some point
            size_t sdFileReadStartIndex = chunkIndex * parent.chunkLength;
            size_t sdFileReadEndIndex = sdFileReadStartIndex + parent.chunkLength;
            auto lengthIt = parent.availableSamples.find(msg.sampleIdentifier);
            if(lengthIt == parent.availableSamples.end()){
                throw std::runtime_error("AudioStreamer::stream: sample to stream not in available samples.");
            }
            size_t sampleLengthInFrames = lengthIt->second;
            if(sdFileReadEndIndex > sampleLengthInFrames){
                sdFileReadEndIndex = sampleLengthInFrames;
                //TODO: check if audiofileutiilities::getsamples is not overwriting the end of sample sentinels.
                for(size_t i = sdFileReadEndIndex - sdFileReadStartIndex; i < parent.chunkLength; i++){
                    streamBuffer.at(i) = END_OF_SAMPLE;
                }
            }
            if(AudioFileUtilities::getSamples(parent.filename(msg.sampleIdentifier),
                        streamBuffer.data(), channel,
                        sdFileReadStartIndex, sdFileReadEndIndex) == 0) {
                for(size_t i = 0; i < parent.chunkLength; i++){
                    parent.chunks.at(chunkIndexInBuffer).at(i) = streamBuffer.at(i);
                }
                parent.chunkStates[chunkIndexInBuffer].set(msg.sampleIdentifier, chunkIndex, true);
                streamToAudio.push({StreamingMessageType::ChunkReady, msg.sampleIdentifier, chunkIndex, chunkIndexInBuffer});
                if(chunkIndex < chunkIndicesInBuffer.size()){
                    chunkIndicesInBuffer.at(chunkIndex) = chunkIndexInBuffer;
                } else if(chunkIndex == chunkIndicesInBuffer.size()){
                    chunkIndicesInBuffer.push_back(chunkIndexInBuffer);
                    DEBUG_PRINTF("\n\n\nWARNING\n\n\n\n\nmutating chunkIndicesInBuffer. exceeded expectedlength\n");
                } else {
                    throw std::runtime_error("AudioStreamer::streamOnThread streaming chunks not in regular order. Not implemented yet and should generally not happen case 1.\n");
                }
            } else {
                DEBUG_PRINTF("failed to load sample %s\n", parent.filename(msg.sampleIdentifier).c_str());
                throw std::runtime_error("StreamingBuffer::initForFolder: failed to load sample");
            }
        }
        if(msg.type == StreamingMessageType::AssignChunk){
            if(chunkIndex < chunkIndicesInBuffer.size()){
                chunkIndicesInBuffer.at(chunkIndex) = chunkIndexInBuffer;
            } else if(chunkIndex == chunkIndicesInBuffer.size()){
                chunkIndicesInBuffer.push_back(chunkIndexInBuffer);
                DEBUG_PRINTF("\n\n\nWARNING\n\n\n\n\nmutating chunkIndicesInBuffer. exceeded expectedmaxlength v2\n");
            } else {
                throw std::runtime_error("AudioStreamer::streamOnThread streaming chunks not in regular order. Not implemented yet and should generally not happen case 2.\n");
            }
        }

        if(parent.mutateTaskInFlight.load(std::memory_order_acquire)){
            streamingTaskInFlight.store(false, std::memory_order_release);
            return;
        }
    }
    streamingTaskInFlight.store(false, std::memory_order_release);
}

void AudioStreamer::flushToDisk(SampleIdentifier sampleIdentifier){ // audiothread sending messages to flush thread
    flushNeedsScheduling = true;
    //A possible improvement would be to only protect chunks that are not flushed yet to have more space
    //to write to but that is not necessary at the moment because space is not an issue
    pendingFlushes.insert(sampleIdentifier);
    parent.protectSample(sampleIdentifier);
    std::vector<int>& chunkIndicesInBuffer = parent.chunkIndicesInBuffer[sampleIdentifier];
    int numberOfChunks = chunkIndicesInBuffer.size();
    audioToFlush.push({StreamingMessageType::FlushInfo, sampleIdentifier, numberOfChunks, numberOfChunks});
    flushNeedsScheduling = true;
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer != CHUNK_INVALID && "AudioStreamer::flushToDisk trying to flush invalid chunk, should not happen");
        audioToFlush.push({StreamingMessageType::FlushChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer});
        flushNeedsScheduling = true;
    }
}

void flushOnThread(void* arg){
    DEBUG_PRINTF("\n\flushOnThread\n\n");
    AudioStreamer* audioStreamer = static_cast<AudioStreamer*>(arg);
    audioStreamer->flush();
}

void AudioStreamer::flush(){ //flush thread only
    StreamingMessage msg;
    std::unordered_map<SampleIdentifier, WavWriter> wavWriters;
    while(audioToFlush.pop(msg)){
        if(parent.mutateTaskInFlight.load(std::memory_order_acquire)){
            flushTaskInFlight.store(false, std::memory_order_release);
            return;
        }
        if(msg.type == StreamingMessageType::FlushInfo){
            wavWriters[msg.sampleIdentifier] = WavWriter(parent.filename(msg.sampleIdentifier), parent.resourceManager, msg.chunkIndex * parent.chunkLength);
            f_numberOfFlushableChunks[msg.sampleIdentifier] = msg.chunkIndex;
        }
        if(msg.type == StreamingMessageType::FlushChunk){
            //potential concurrent read, however not so bad since 1 the chunk is still prtected to will not be written to and 2 with only 1 core
            //on the cpu i think concurrent reads on a constant array should not make too many problems.
            for(size_t i = 0; i < parent.chunkLength; i++){
                float nextFrame = parent.chunks.at(msg.chunkIndexInBuffer).at(i);
                if(nextFrame == END_OF_SAMPLE){
                    flushTaskInFlight.store(false, std::memory_order_release);
                    return;
                }
                flushBuffer.at(i) = nextFrame;
            }
            wavWriters[msg.sampleIdentifier].writeChunk(msg.chunkIndex * parent.chunkLength, flushBuffer);
            if(msg.chunkIndex == f_numberOfFlushableChunks[msg.sampleIdentifier] - 1){
                wavWriters.erase(msg.sampleIdentifier);
                flushToAudio.push({StreamingMessageType::FlushComplete, msg.sampleIdentifier, -1, -1});
            }
        }
    }
    flushTaskInFlight.store(false, std::memory_order_release);
}

void AudioStreamer::processBlockwise(){
    /////////////////////Message passing part.
    StreamingMessage msg;
    while(streamToAudio.pop(msg)){
        std::vector<int>& chunkIndicesInBuffer = parent.chunkIndicesInBuffer[msg.sampleIdentifier];
        if(msg.type == StreamingMessageType::ChunkReady){
            if(msg.chunkIndex < chunkIndicesInBuffer.size()){
                chunkIndicesInBuffer.at(msg.chunkIndex) = msg.chunkIndexInBuffer;
            } else if(msg.chunkIndex == chunkIndicesInBuffer.size()){
                chunkIndicesInBuffer.push_back(msg.chunkIndexInBuffer);
                DEBUG_PRINTF("\n\n\nWARNING\n\n\n\n\nmutating chunkIndicesInBuffer. exceeded expectedlength v3\n");
            } else {
                throw std::runtime_error("StreamingBuffer::processBlockwise Receiving chunks not in regular order. Not implemented yet and should generally not happen.\n");
            }
        }
        if(msg.type == StreamingMessageType::InvalidatedChunk){
            chunkIndicesInBuffer.at(msg.chunkIndex) = CHUNK_INVALID;
        }
    }
    while(flushToAudio.pop(msg)){
        if(msg.type == StreamingMessageType::FlushComplete){
            pendingFlushes.erase(msg.sampleIdentifier);
            //Is there an iterator living on the sample? if not unprotect it.
            bool exists = std::any_of(
                parent.iterators.begin(),
                parent.iterators.end(),
                [&](const StreamingBufferIterator& iter) {
                    return iter.sampleIdentifier == msg.sampleIdentifier;
                }
            );

            if (!exists) {
                // No iterator there -> no need to protect anymore
                parent.unProtectSample(msg.sampleIdentifier);
            }
        }
    }
    ///////////////////////task management part
    if(streamNeedsScheduling && !parent.mutateTaskInFlight.load(std::memory_order_acquire)){
        bool expected = false;
        if(streamingTaskInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel)){
            int scheduleResponse = Bela_scheduleAuxiliaryTask(streamSamplesTask);
            if(scheduleResponse == 0){
                streamNeedsScheduling = false;
            } else {
                streamingTaskInFlight.store(false, std::memory_order_release);
                if(scheduleResponse != EBUSY){
                    DEBUG_RT_PRINTF("Audiostreamer::processBlockwise(): Bela_scheduleAuxiliaryTask sent error code: %d\n", scheduleResponse);
                }
            }
        }
    }
    if(flushNeedsScheduling && !parent.mutateTaskInFlight.load(std::memory_order_acquire)){
        bool expected = false;
        if(flushTaskInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel)){
            int scheduleResponse = Bela_scheduleAuxiliaryTask(flushToDiskTask);
            if(scheduleResponse == 0){
                flushNeedsScheduling = false;
            } else {
                flushTaskInFlight.store(false, std::memory_order_release);
                if(scheduleResponse != EBUSY){
                    DEBUG_RT_PRINTF("Audiostreamer::processBlockwise(): Bela_scheduleAuxiliaryTask sent error code: %d\n", scheduleResponse);
                }
            }
        }
    }
}
