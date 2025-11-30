#include "../include/StreamingBuffer.h"
#include "../include/WavWriter.h"
#include <errno.h>
//compile


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
    assert(messages.size() > currentHead && "StreamingMessageQueue::push currentHead out of range");
    messages.at(currentHead) = value;
    DEBUG_PRINTF("SMQ::push type=%d sample=%d_%d chunk=%d chunkBuf=%d head=%zu->%zu tail=%zu\n",
        static_cast<int>(value.type),
        value.sampleIdentifier.first, value.sampleIdentifier.second,
        value.chunkIndex, value.chunkIndexInBuffer,
        currentHead, nextHead, tail.load(std::memory_order_relaxed));
    head.store(nextHead, std::memory_order_release);
    return true;
}

bool StreamingMessageQueue::pop(StreamingMessage& out) {
    size_t currentTail = tail.load(std::memory_order_relaxed);
    size_t currentHead = head.load(std::memory_order_acquire);
    if(currentTail == currentHead)
        return false;
    DEBUG_PRINTF("messages.size() = %zu\n", messages.size());
    assert(messages.size() > currentTail && "StreamingMessageQueue::pop currentTail out of range");
    out = messages.at(currentTail);
        DEBUG_PRINTF("SMQ::pop type=%d sample=%d_%d chunk=%d chunkBuf=%d head=%zu tail=%zu\n",
        static_cast<int>(out.type),
        out.sampleIdentifier.first, out.sampleIdentifier.second,
        out.chunkIndex, out.chunkIndexInBuffer,
        currentHead, currentTail);
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
        iterator.parent->eraseIterator(iterator);
    }
    *(iterator.data) = value;
    return *this;
}


ElementProxy::operator float() const {
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        return END_OF_SAMPLE;
    }
    if(*(iterator.data) == END_OF_SAMPLE){
        iterator.parent->eraseIterator(iterator);
    }
    return *(iterator.data);
}

///////////StreamingBufferIterator///////////

namespace {
    std::vector<int> EMPTY_CHUNK_INDEX_VECTOR = std::vector<int>();
}
StreamingBufferIterator::StreamingBufferIterator(StreamingBuffer* parent)
        : sampleIdentifier({ITERATOR_INVALID,ITERATOR_INVALID}),
        index(0),
        parent(parent),
        chunkIndicesInBuffer(&EMPTY_CHUNK_INDEX_VECTOR),
        type(SBIType::None),
        chunkIndex(ITERATOR_INVALID),
        indexInChunk(ITERATOR_INVALID),
        chunkIndexInBuffer(ITERATOR_INVALID),
        chunkStartPtr(nullptr),
        data(nullptr){}

StreamingBufferIterator::StreamingBufferIterator(SampleIdentifier sampleIdentifier, size_t index,
                                            StreamingBuffer* parent, std::vector<int>* chunkIndicesInBuffer,
                                            SBIType type)
        : sampleIdentifier(sampleIdentifier),
        index(index),
        parent(parent),
        chunkIndicesInBuffer(chunkIndicesInBuffer),
        type(type),
        chunkIndex(index / parent->chunkLength),
        indexInChunk(index % parent->chunkLength),
        chunkIndexInBuffer(0),
        chunkStartPtr(nullptr),
        data(nullptr) {
    assert(chunkIndicesInBuffer != nullptr && "StreamingBufferIterator ctor chunkIndicesInBuffer null");
    assert(chunkIndex < chunkIndicesInBuffer->size() && "StreamingBufferIterator ctor chunkIndex out of range");
    chunkIndexInBuffer = chunkIndicesInBuffer->at(chunkIndex);
    assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent->chunks.size() && "StreamingBufferIterator ctor chunkIndexInBuffer out of range");
    chunkStartPtr = parent->chunks.at(chunkIndexInBuffer).data();
    data = chunkStartPtr + indexInChunk;
}

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
    if(indexInChunk >= parent->chunkLength){
        ++chunkIndex;
        indexInChunk = 0;
        if(chunkIndicesInBuffer->size() <= static_cast<size_t>(chunkIndex)){
            throw std::runtime_error("StreamingBufferIterator::operator++: iterator incremented past end of sample\n");
        } else {
            assert(chunkIndex < chunkIndicesInBuffer->size() && "StreamingBufferIterator::operator++ chunkIndex out of range before read");
            chunkIndexInBuffer = chunkIndicesInBuffer->at(chunkIndex);
            if(chunkIndexInBuffer == CHUNK_INVALID){
                if(type == SBIType::Write){
                    assert(chunkIndex < chunkIndicesInBuffer->size() && "StreamingBufferIterator::operator++ write chunkIndex out of range before assign");
                    chunkIndexInBuffer = parent->findFreeChunk();
                    chunkIndicesInBuffer->at(chunkIndex) = chunkIndexInBuffer;
                    parent->audioStreamer.audioToStream.push({StreamingMessageType::AssignChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer});
                    parent->audioStreamer.streamNeedsScheduling = true;
                    parent->chunkStates[chunkIndexInBuffer].set(sampleIdentifier, chunkIndex, true);
                }
                if(type == SBIType::Read){
                    throw std::runtime_error("StreamingBufferIterator::operator++: stream too slow");
                }
            }
            assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent->chunks.size() && "StreamingBufferIterator::operator++ chunkIndexInBuffer out of range");
            chunkStartPtr = parent->chunks.at(chunkIndexInBuffer).data();
            data = chunkStartPtr;
        }
    } else {
        ++data;
    }
    return *this;
}

void StreamingBufferIterator::set(SampleIdentifier sampleIdentifier,
        size_t index, StreamingBuffer* parent,
        std::vector<int>* chunkIndicesInBuffer,
        SBIType type){
    this->sampleIdentifier = sampleIdentifier;
    this->index = index;
    this->parent = parent;
    this->chunkIndicesInBuffer = chunkIndicesInBuffer;
    this->type = type;
    this->chunkIndex = index / parent->chunkLength;
    this->indexInChunk = index % parent->chunkLength;
    assert(chunkIndicesInBuffer != nullptr && "StreamingBufferIterator::set chunkIndicesInBuffer null");
    assert(chunkIndex < chunkIndicesInBuffer->size() && "StreamingBufferIterator::set chunkIndex out of range");
    this->chunkIndexInBuffer = chunkIndicesInBuffer->at(chunkIndex);
if (chunkIndexInBuffer < 0 ||
    static_cast<size_t>(chunkIndexInBuffer) >= parent->chunks.size()) {
    std::string errormsg = "chunkIndexInBuffer " + std::to_string(chunkIndexInBuffer) +
                           " of " + std::to_string(parent->chunks.size());
    throw std::runtime_error(errormsg);
}
    assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent->chunks.size() && "StreamingBufferIterator::set chunkIndexInBuffer out of range");
    this->chunkStartPtr = parent->chunks.at(chunkIndexInBuffer).data();
    this->data = chunkStartPtr + indexInChunk;
}

void StreamingBufferIterator::initialize(){
    this->sampleIdentifier = {ITERATOR_INVALID,ITERATOR_INVALID};
    this->index = 0;
    this->chunkIndicesInBuffer = &EMPTY_CHUNK_INDEX_VECTOR;
    this->type = SBIType::None;
    this->chunkIndex = 0;
    this->indexInChunk = 0;
    this->chunkIndexInBuffer = ITERATOR_INVALID;
    this->chunkStartPtr = nullptr;
    this->data = nullptr;
}

///////////StreamingBuffer///////////

StreamingBuffer::StreamingBuffer(ResourceManager* resourceManager,
            size_t bufferLengthInFrames,
            int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples)
            : resourceManager(resourceManager),
            bufferName(bufferName), folderPath(folderPath),
            chunkLength(8820),
            totalNumberOfChunks(static_cast<int>(bufferLengthInFrames / chunkLength) + 1),
            totalNumberOfIterators(totalNumberOfIterators),
            availableSamples(availableSamples),
            iterators(std::vector<StreamingBufferIterator>(totalNumberOfIterators, StreamingBufferIterator(this))),
            audioStreamer(*this),
            chunkStates(new ChunkState[totalNumberOfChunks]),
            chunks(totalNumberOfChunks, std::vector<float>(chunkLength)),
            mutateDataTaskName(bufferName + "_MTask"),
            MutateDataTask(Bela_createAuxiliaryTask(mutateOnThread, mutateDataPrio, mutateDataTaskName.c_str(), (void*)this))
{
    DEBUG_PRINTF("StreamingBuffer ctor(dynamic): bufferFrames=%u chunkLen=%d totalChunks=%d iters=%d name=%s path=%s fps=%d\n",
        (unsigned int)bufferLengthInFrames,
        chunkLength,
        totalNumberOfChunks,
        totalNumberOfIterators,
        bufferName.c_str(),
        folderPath.c_str(),
        resourceManager->audioFramesPerSecond);
}

StreamingBuffer::~StreamingBuffer(){
    delete[] chunkStates;
}

void mutateOnThread(void* arg){
    DEBUG_PRINTF("\n\n mutateOnThread\n\n");
    StreamingBuffer* streamingBuffer = static_cast<StreamingBuffer*>(arg);
    streamingBuffer->mutate();
}

void StreamingBuffer::mutate(){
    StreamingMessage msg;
    while(audioToMutate.pop(msg)){
        if(msg.type == StreamingMessageType::InitializeSample){
            auto insertResult = chunkIndicesInBuffer.insert(std::make_pair(msg.sampleIdentifier, std::vector<int>()));
            std::vector<int>& localChunkIndicesInBuffer = insertResult.first->second;
            DEBUG_PRINTF("StreamingBuffer::mutate InitializeSample %d_%d chunks=%d%s\n",
                msg.sampleIdentifier.first, msg.sampleIdentifier.second, msg.chunkIndex,
                insertResult.second ? " (new)" : " (reset)");
            if(!insertResult.second){
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
            DEBUG_PRINTF("StreamingBuffer::mutate Clear\n");
            chunkIndicesInBuffer.clear();
            audioStreamer.s_chunkIndicesInBuffer.clear();
            audioStreamer.pendingFlushes.clear();
            audioStreamer.f_numberOfFlushableChunks.clear();
            for(StreamingBufferIterator& it : iterators){
                it.initialize();
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
    auto insertResult = chunkIndicesInBuffer.insert(std::make_pair(sampleIdentifier, std::vector<int>()));
    bool inserted = insertResult.second;
    std::vector<int>& localChunkIndicesInBuffer = insertResult.first->second;
    DEBUG_PRINTF("StreamingBuffer::begin sample %d_%d type=%d inserted=%d chunkindicesinbuffer.size() = %zu\n",
        sampleIdentifier.first, sampleIdentifier.second, static_cast<int>(type), inserted ? 1 : 0, localChunkIndicesInBuffer.size());
    for(int i = 0; i < totalNumberOfIterators; i++){
        StreamingBufferIteratorAssignIndex = (StreamingBufferIteratorAssignIndex + 1) % totalNumberOfIterators;
        DEBUG_PRINTF("trying to write to iterator %zu with key %d\n", StreamingBufferIteratorAssignIndex, iterators.at(StreamingBufferIteratorAssignIndex).sampleIdentifier.first);
        assert(StreamingBufferIteratorAssignIndex < iterators.size() && "StreamingBuffer::begin iterator index out of range");
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
    assert(StreamingBufferIteratorAssignIndex < iterators.size() && "StreamingBuffer::begin assign index out of range");
    assert(localChunkIndicesInBuffer.size() > 0 && "StreamingBuffer::begin: chunkIndicesInBuffer not initialized for sampleIdentifier");
    assert(0 < localChunkIndicesInBuffer.size() && "StreamingBuffer::begin chunk 0 missing");
    if (localChunkIndicesInBuffer.at(0) == CHUNK_INVALID) {
        int chunkIndexInBuffer = findFreeChunk();
        assert(0 < localChunkIndicesInBuffer.size() && "StreamingBuffer::begin chunk 0 missing before assign");
        localChunkIndicesInBuffer.at(0) = chunkIndexInBuffer;
        audioStreamer.audioToStream.push({StreamingMessageType::AssignChunk, sampleIdentifier, 0, chunkIndexInBuffer});
        audioStreamer.streamNeedsScheduling = true;
        if(type == SBIType::Read){
            assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < chunks.size() && "StreamingBuffer::begin chunkIndexInBuffer out of range before write");
            assert(chunks.at(chunkIndexInBuffer).size() > 0 && "StreamingBuffer::begin chunk data empty at 0");
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
    iterators.at(StreamingBufferIteratorAssignIndex).set(sampleIdentifier, 0, this, &localChunkIndicesInBuffer, type);
    assert(StreamingBufferIteratorAssignIndex < iterators.size() && "StreamingBuffer::begin return index out of range");
    return iterators.at(StreamingBufferIteratorAssignIndex);
}

void StreamingBuffer::streamStarts(){
    for(auto& sampleInfo: availableSamples){
        SampleIdentifier sampleIdentifier = sampleInfo.first;
        audioStreamer.streamStartFromDisk(sampleIdentifier);
    }
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
        itRef.initialize();
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
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < localChunkIndicesInBuffer.size() && "protectsample: at(chunkIndex) out of range");
        if(localChunkIndicesInBuffer.at(chunkIndex) == CHUNK_INVALID){
            continue;
        }
        int chunkIndexInBuffer = localChunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < chunks.size() && "protectsample: chunkIndexInBuffer out of range");
        chunkStates[chunkIndexInBuffer].writeProtected.store(true, std::memory_order_release);
    }
}

void StreamingBuffer::unProtectSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& localChunkIndicesInBuffer = chunkIndicesInBuffer[sampleIdentifier];
    for(int chunkIndex = 0; chunkIndex < localChunkIndicesInBuffer.size(); chunkIndex++){
        assert(localChunkIndicesInBuffer.size() > chunkIndex && "unprotectsample: chunkindicesinbuffer to small");
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < localChunkIndicesInBuffer.size() && "unprotectsample: at(chunkIndex) out of range");
        if(localChunkIndicesInBuffer.at(chunkIndex) == CHUNK_INVALID){
            continue;
        }
        int chunkIndexInBuffer = localChunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < chunks.size() && "unprotectsample: chunkIndexInBuffer out of range");
        chunkStates[chunkIndexInBuffer].writeProtected.store(false, std::memory_order_release);
    }
}

void StreamingBuffer::initializeSamples(std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    //DEBUG_PRINTF("StreamingBuffer::initializeSamples count=%zu\n", availableSamples.size());
    clear();
    for(auto& sampleInfo: availableSamples){
        initializeSample(sampleInfo.first, sampleInfo.second);
    }
}

void StreamingBuffer::initializeSample(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames){
    //DEBUG_PRINTF("StreamingBuffer::initializeSample %d_%d frames=%u\n",
    //    sampleIdentifier.first, sampleIdentifier.second, (unsigned int)expectedSampleLengthInFrames);
    int numberOfChunks = static_cast<int>(expectedSampleLengthInFrames / chunkLength) + 1;
    audioToMutate.push({StreamingMessageType::InitializeSample, sampleIdentifier, numberOfChunks, -1});
    mutateDataTaskNeedsScheduling = true;
}

void StreamingBuffer::printInfo(){
    rt_printf("StreamingBuffer info:\n");
    rt_printf("  name=%s path=%s\n", bufferName.c_str(), folderPath.c_str());
    rt_printf("  chunkLength=%d totalChunks=%d iterators=%d\n", chunkLength, totalNumberOfChunks, totalNumberOfIterators);
    rt_printf("  chunks size=%zu chunkStates=%s\n", chunks.size(), chunkStates ? "yes" : "no");
    rt_printf("  availableSamples=%zu chunkIndicesInBuffer=%zu\n", availableSamples.size(), chunkIndicesInBuffer.size());
    for(auto& availableSample: availableSamples){
        SampleIdentifier sampleIdentifier = availableSample.first;
        rt_printf("chunkIndicesInBuffer[%d,%d] = {%d, %d, ...\n", sampleIdentifier.first, sampleIdentifier.second, chunkIndicesInBuffer[sampleIdentifier].at(0), chunkIndicesInBuffer[sampleIdentifier].at(1));

    }
    rt_printf("...\n");
    rt_printf("  iteratorAssignIndex=%zu freeChunkIdx=%zu\n", StreamingBufferIteratorAssignIndex, freeChunkSearchIdx.load(std::memory_order_relaxed));
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
        DEBUG_PRINTF("trying to write to chunkindexinbuffer: %zu where chunkIndex is %d\n", chunkIndexInBuffer, chunkIndex);
        if(chunkIndex != 0){ //NEVER OVERWRITE A FIRST CHUNK

            if(!( chunkStates[chunkIndexInBuffer].writeProtected.load(std::memory_order_acquire) )){
                            DEBUG_PRINTF("returning free chunk %d\n", chunkIndexInBuffer);
                return static_cast<int>(chunkIndexInBuffer);
            }
        }
    }
    throw std::runtime_error("did not find a free chunk\n");
    return -1;
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

void AudioStreamer::streamChunk(SampleIdentifier sampleIdentifier, int chunkIndex, int chunkIndexInBuffer){
    DEBUG_PRINTF("chunkindexinbuffer = %d\n", chunkIndexInBuffer);
    if(chunkIndexInBuffer == CHUNK_INVALID){
        audioToStream.push({StreamingMessageType::StreamChunk, sampleIdentifier, chunkIndex, -1});
        streamNeedsScheduling = true;
    } else {
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::streamChunk chunkIndexInBuffer out of range");
        assert(parent.chunks.at(chunkIndexInBuffer).size() > 0 && "AudioStreamer::streamChunk chunk empty at read");
        if(parent.chunks.at(chunkIndexInBuffer).at(0) == SAMPLE_NOT_LOADED_VALUE){
            audioToStream.push({StreamingMessageType::StreamChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer});
            streamNeedsScheduling = true;
        }
    }
}

void AudioStreamer::streamFromDisk(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = parent.chunkIndicesInBuffer[sampleIdentifier];
    int numberOfChunks = chunkIndicesInBuffer.size();
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "AudioStreamer::streamFromDisk chunkIndex out of range");
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        streamChunk(sampleIdentifier, chunkIndex, chunkIndexInBuffer);
    }
}

void AudioStreamer::streamStartFromDisk(SampleIdentifier sampleIdentifier){
    assert(parent.chunkIndicesInBuffer[sampleIdentifier].size() > 0 && "AudioStreamer::streamStartFromDisk missing chunk 0");
    int chunkIndexInBuffer = parent.chunkIndicesInBuffer[sampleIdentifier].at(0);
    streamChunk(sampleIdentifier, 0, chunkIndexInBuffer);
}

void streamOnThread(void* arg){
    DEBUG_PRINTF("\n\n streamOnThread\n\n");
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
                    assert(streamBuffer.size() > i && "AudioStreamer::stream streamBuffer sentinel write out of range");
                    streamBuffer.at(i) = END_OF_SAMPLE;
                }
            }
            if(AudioFileUtilities::getSamples(parent.filename(msg.sampleIdentifier),
                        streamBuffer.data(), channel,
                        sdFileReadStartIndex, sdFileReadEndIndex) == 0) {
                assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::stream chunkIndexInBuffer out of range during copy");
                for(size_t i = 0; i < parent.chunkLength; i++){
                    assert(parent.chunks.at(chunkIndexInBuffer).size() > i && "AudioStreamer::stream parent chunk write out of range");
                    assert(streamBuffer.size() > i && "AudioStreamer::stream streamBuffer out of range");
                    parent.chunks.at(chunkIndexInBuffer).at(i) = streamBuffer.at(i);
                }
                parent.chunkStates[chunkIndexInBuffer].set(msg.sampleIdentifier, chunkIndex, true);
                streamToAudio.push({StreamingMessageType::ChunkReady, msg.sampleIdentifier, chunkIndex, chunkIndexInBuffer});
                if(chunkIndex < chunkIndicesInBuffer.size()){
                    assert(chunkIndex < chunkIndicesInBuffer.size() && "AudioStreamer::stream chunkIndex out of range after load");
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
                assert(chunkIndex < chunkIndicesInBuffer.size() && "AudioStreamer::stream AssignChunk index out of range");
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
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "AudioStreamer::flushToDisk chunkIndex out of range");
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer != CHUNK_INVALID && "AudioStreamer::flushToDisk trying to flush invalid chunk, should not happen");
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::flushToDisk chunkIndexInBuffer out of range");
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
            wavWriters.insert(std::make_pair(
                msg.sampleIdentifier,
                WavWriter(parent.filename(msg.sampleIdentifier), parent.resourceManager, msg.chunkIndex * parent.chunkLength)
            ));
            f_numberOfFlushableChunks[msg.sampleIdentifier] = msg.chunkIndex;
        }
        if(msg.type == StreamingMessageType::FlushChunk){
            //potential concurrent read, however not so bad since 1 the chunk is still prtected to will not be written to and 2 with only 1 core
            //on the cpu i think concurrent reads on a constant array should not make too many problems.
            auto writerIt = wavWriters.find(msg.sampleIdentifier);
            if(writerIt == wavWriters.end()){
                throw std::runtime_error("AudioStreamer::flush: no wav writer for sampleIdentifier");
            }
            for(size_t i = 0; i < parent.chunkLength; i++){
                assert(msg.chunkIndexInBuffer >= 0 && static_cast<size_t>(msg.chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::flush chunkIndexInBuffer out of range");
                assert(parent.chunks.at(msg.chunkIndexInBuffer).size() > i && "AudioStreamer::flush chunk data out of range");
                float nextFrame = parent.chunks.at(msg.chunkIndexInBuffer).at(i);
                if(nextFrame == END_OF_SAMPLE){
                    flushTaskInFlight.store(false, std::memory_order_release);
                    return;
                }
                assert(flushBuffer.size() > i && "AudioStreamer::flush flushBuffer out of range");
                flushBuffer.at(i) = nextFrame;
            }
            writerIt->second.writeChunk(msg.chunkIndex * parent.chunkLength, flushBuffer);
            if(msg.chunkIndex == f_numberOfFlushableChunks[msg.sampleIdentifier] - 1){
                wavWriters.erase(writerIt);
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
                assert(msg.chunkIndex < chunkIndicesInBuffer.size() && "AudioStreamer::processBlockwise ChunkReady index out of range");
                chunkIndicesInBuffer.at(msg.chunkIndex) = msg.chunkIndexInBuffer;
            } else if(msg.chunkIndex == chunkIndicesInBuffer.size()){
                chunkIndicesInBuffer.push_back(msg.chunkIndexInBuffer);
                DEBUG_PRINTF("\n\n\nWARNING\n\n\n\n\nmutating chunkIndicesInBuffer. exceeded expectedlength v3\n");
            } else {
                throw std::runtime_error("StreamingBuffer::processBlockwise Receiving chunks not in regular order. Not implemented yet and should generally not happen.\n");
            }
        }
        if(msg.type == StreamingMessageType::InvalidatedChunk){
            assert(msg.chunkIndex < chunkIndicesInBuffer.size() && "AudioStreamer::processBlockwise InvalidatedChunk index out of range");
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
