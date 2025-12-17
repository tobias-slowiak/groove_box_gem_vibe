#include "../include/StreamingBuffer.h"


///////////ElementProxy///////////

ElementProxy& ElementProxy::operator=(float value){
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        throw std::runtime_error("writing to invalid Iterator");
    }
    assert(iterator.data != nullptr && "ElementProxy::operator= data null");
    if(value == END_OF_SAMPLE){
        //TODO set sample length here if not set yet
        if(*(iterator.data) != END_OF_SAMPLE){
            throw std::runtime_error("ElementProxy::operator= write iterator tries to change length of sample -> initializeSample before!\n");
        }
        iterator.release();
    }
    *(iterator.data) = value;
    return *this;
}


ElementProxy::operator float() const {
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        throw std::runtime_error("reading from invalid iterator");
    }
    assert(iterator.data != nullptr && "ElementProxy::operator float data null");
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
        streamingAdvanceInChunks(DEFAULT_STREAMING_ADVANCE_IN_CHUNKS),
        sampleLength(0),
        chunkIndex(ITERATOR_INVALID),
        indexInChunk(ITERATOR_INVALID),
        chunkIndexInBuffer(ITERATOR_INVALID),
        chunkStartPtr(nullptr),
        data(nullptr){
    assert(parent != nullptr && "StreamingBufferIterator null parent in default ctor");
}


StreamingBufferIterator StreamingBufferIterator::operator++(int){    // post-increment
    StreamingBufferIterator tmp = *this;
    ++(*this);
    return tmp;
}

StreamingBufferIterator& StreamingBufferIterator::operator++(){      // pre-increment
    if(sampleIdentifier.first == ITERATOR_INVALID){
        throw std::runtime_error("using ++ on invalid iterator");
    }
    index++;
    indexInChunk++;
    if(indexInChunk >= parent->chunkLength){
        ++chunkIndex;
        indexInChunk = 0;
        assert(chunkIndexInBuffer < parent->totalNumberOfChunks);
        ChunkState& oldChunkState = parent->chunkStates[chunkIndexInBuffer];
        assert(chunkIndex < chunkIndicesInBuffer->size() && "StreamingBufferIterator::operator++ chunkIndex out of range before read");
        chunkIndexInBuffer = chunkIndicesInBuffer->at(chunkIndex);
        assert(chunkIndexInBuffer < parent->totalNumberOfChunks);
        ChunkState& chunkState = parent->chunkStates[chunkIndexInBuffer];

        if(type == SBIType::Read){
            if(chunkIndex + streamingAdvanceInChunks <= sampleLength / parent->chunkLength){
                int chunkIndexInBufferForStream = chunkIndicesInBuffer->at(chunkIndex + streamingAdvanceInChunks);
                if(chunkIndexInBufferForStream == CHUNK_INVALID)
                    parent->audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex + streamingAdvanceInChunks, *chunkIndicesInBuffer);
            }
            if(chunkIndexInBuffer == CHUNK_INVALID) throw std::runtime_error("apparently the stream chunk message was never sent");
            if(!chunkState.chunkReady.load(std::memory_order_acquire)) throw std::runtime_error("stream too slow");
        }
        if(type == SBIType::Write){
            oldChunkState.chunkReady.store(true, std::memory_order_release); 
            if(chunkIndexInBuffer == CHUNK_INVALID) chunkIndexInBuffer = parent->assignToFreeChunk(sampleIdentifier, chunkIndex, *chunkIndicesInBuffer);            
        }

        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent->chunks.size() && "StreamingBufferIterator::operator++ chunkIndexInBuffer out of range");
        chunkStartPtr = parent->chunks.at(chunkIndexInBuffer).data();
        data = chunkStartPtr;
    } else {
        ++data;
    }
    assert(data != nullptr && "StreamingBufferIterator::operator++ data null after reassignment");
    if(index >= sampleLength){
        ++data;
        assert(*data == END_OF_SAMPLE);
        this->release();
    }
    return *this;
}

void StreamingBufferIterator::set(SampleIdentifier sampleIdentifier,
                                std::vector<int>* chunkIndicesInBuffer,
                                SBIType type,
                                int streamingAdvanceInChunks){
    auto lengthIt = parent->availableSamples.find(sampleIdentifier);
    if(lengthIt == parent->availableSamples.end()){
        throw std::runtime_error("StreamingBufferIterator::set: sampleIdentifier missing from availableSamples");
    }
    this->sampleIdentifier = sampleIdentifier;
    this->index = 0;
    this->chunkIndicesInBuffer = chunkIndicesInBuffer;
    this->type = type;
    this->sampleLength = lengthIt->second;
    assert(this->sampleLength > 0 && "StreamingBufferIterator::set sampleLength must be > 0");

    this->chunkIndex = 0;
    this->indexInChunk = 0;
    assert(chunkIndicesInBuffer != nullptr  && chunkIndicesInBuffer->size() != 0 && "StreamingBufferIterator::set chunkIndicesInBuffer null");
    this->chunkIndexInBuffer = chunkIndicesInBuffer->at(0);
    assert(chunkIndexInBuffer != CHUNK_INVALID && "first chunk has to be ready before iterator");
    assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent->chunks.size() && "StreamingBufferIterator::set chunkIndexInBuffer out of range");
    this->chunkStartPtr = parent->chunks.at(chunkIndexInBuffer).data();
    this->data = chunkStartPtr;
    
    if(this->type == SBIType::Read){
        for(int i = 0; i <= streamingAdvanceInChunks; i++){
            int chunkIndexInBufferForStream = chunkIndicesInBuffer->at(chunkIndex + i);
            if(chunkIndexInBufferForStream == CHUNK_INVALID)
                parent->audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex + i, *chunkIndicesInBuffer);
        }
    }
}

void StreamingBufferIterator::initialize(){
    this->sampleIdentifier = {ITERATOR_INVALID,ITERATOR_INVALID};
    this->index = 0;
    this->chunkIndicesInBuffer = &EMPTY_CHUNK_INDEX_VECTOR;
    this->type = SBIType::None;
    this->sampleLength = 0;
    this->chunkIndex = 0;
    this->indexInChunk = 0;
    this->chunkIndexInBuffer = ITERATOR_INVALID;
    this->chunkStartPtr = nullptr;
    this->data = nullptr;
}

void StreamingBufferIterator::release(){
    parent->releaseIterator(*this);
}