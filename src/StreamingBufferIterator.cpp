#include "../include/StreamingBuffer.h"

static constexpr int ITERATOR_ADVANCE_IN_BLOCKS = 5;

///////////ElementProxy///////////

ElementProxy& ElementProxy::operator=(float value){
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        DEBUG_RT_PRINTF("ElementProxy::operator=: WARNING: trying to write to invalid iterator\n");
        return *this;
    }
    assert(iterator.parent != nullptr && "ElementProxy::operator= parent null");
    assert(iterator.data != nullptr && "ElementProxy::operator= data null");
    assert(iterator.chunkIndicesInBuffer != nullptr && "ElementProxy::operator= chunkIndicesInBuffer null");
    assert(iterator.chunkIndexInBuffer != ITERATOR_INVALID && "ElementProxy::operator= chunkIndexInBuffer invalid");
    assert(iterator.chunkStartPtr != nullptr && "ElementProxy::operator= chunkStartPtr null");
    assert(iterator.chunkIndex >= 0 && "ElementProxy::operator= chunkIndex negative");
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
    assert(iterator.parent != nullptr && "ElementProxy::operator float parent null");
    assert(iterator.data != nullptr && "ElementProxy::operator float data null");
    assert(iterator.chunkIndicesInBuffer != nullptr && "ElementProxy::operator float chunkIndicesInBuffer null");
    assert(iterator.chunkIndexInBuffer != ITERATOR_INVALID && "ElementProxy::operator float chunkIndexInBuffer invalid");
    assert(iterator.chunkStartPtr != nullptr && "ElementProxy::operator float chunkStartPtr null");
    assert(iterator.chunkIndex >= 0 && "ElementProxy::operator float chunkIndex negative");
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
        sampleLength(0),
        chunkIndex(ITERATOR_INVALID),
        indexInChunk(ITERATOR_INVALID),
        chunkIndexInBuffer(ITERATOR_INVALID),
        chunkStartPtr(nullptr),
        data(nullptr){
    assert(parent != nullptr && "StreamingBufferIterator null parent in default ctor");
}

StreamingBufferIterator::StreamingBufferIterator(SampleIdentifier sampleIdentifier, size_t index,
                                            StreamingBuffer* parent, std::vector<int>* chunkIndicesInBuffer,
                                            SBIType type)
        : sampleIdentifier(sampleIdentifier),
        index(index),
        parent(parent),
        chunkIndicesInBuffer(chunkIndicesInBuffer),
        type(type),
        sampleLength(0),
        chunkIndex(index / parent->chunkLength),
        indexInChunk(index % parent->chunkLength),
        chunkIndexInBuffer(0),
        chunkStartPtr(nullptr),
        data(nullptr) {
    assert(parent != nullptr && "StreamingBufferIterator ctor parent null");
    assert(chunkIndicesInBuffer != nullptr && "StreamingBufferIterator ctor chunkIndicesInBuffer null");
    auto lengthIt = parent->availableSamples.find(sampleIdentifier);
    if(lengthIt == parent->availableSamples.end()){
        throw std::runtime_error("StreamingBufferIterator ctor: sampleIdentifier missing from availableSamples");
    }
    sampleLength = lengthIt->second;
    assert(sampleLength > 0 && "StreamingBufferIterator ctor sampleLength must be > 0");
    assert(indexInChunk < parent->chunkLength && "StreamingBufferIterator ctor indexInChunk out of range");
    assert(chunkIndex >= 0 && "StreamingBufferIterator ctor chunkIndex negative");
    assert(chunkIndex < chunkIndicesInBuffer->size() && "StreamingBufferIterator ctor chunkIndex out of range");
    chunkIndexInBuffer = chunkIndicesInBuffer->at(chunkIndex);
    assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent->chunks.size() && "StreamingBufferIterator ctor chunkIndexInBuffer out of range");
    chunkStartPtr = parent->chunks.at(chunkIndexInBuffer).data();
    data = chunkStartPtr + indexInChunk;
    assert(chunkStartPtr != nullptr && "StreamingBufferIterator ctor chunkStartPtr null");
    assert(data != nullptr && "StreamingBufferIterator ctor data null");

    if(type == SBIType::Read){
        for(int i = 0; i < ITERATOR_ADVANCE_IN_BLOCKS; i++){
            int indexToStream = chunkIndex + i;
            int chunkIndexInBufferForStream = chunkIndicesInBuffer->at(indexToStream);
            if(chunkIndexInBufferForStream == CHUNK_INVALID)
                parent->audioStreamer.sendStreamChunkMessage(sampleIdentifier, indexToStream);
        }
    }
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
    assert(parent != nullptr && "StreamingBufferIterator::operator++ parent null");
    assert(chunkIndicesInBuffer != nullptr && "StreamingBufferIterator::operator++ chunkIndicesInBuffer null");
    assert(chunkStartPtr != nullptr && "StreamingBufferIterator::operator++ chunkStartPtr null");
    assert(data != nullptr && "StreamingBufferIterator::operator++ data null");
    assert(chunkIndex != ITERATOR_INVALID && "StreamingBufferIterator::operator++ chunkIndex invalid");
    assert(indexInChunk != ITERATOR_INVALID && "StreamingBufferIterator::operator++ indexInChunk invalid");
    ++index;
    if(index >= sampleLength - 10000){
        DEBUG_RT_PRINTF("found end of sample\n");
        this->initialize();
    }
    ++indexInChunk;
    if(indexInChunk >= parent->chunkLength){
        ++chunkIndex;
        //TODO: deal with case of sampleLength%chunkLength ==0
        if(chunkIndex + ITERATOR_ADVANCE_IN_BLOCKS <= sampleLength / parent->chunkLength){
            int chunkIndexInBufferForStream = chunkIndicesInBuffer->at(chunkIndex + ITERATOR_ADVANCE_IN_BLOCKS);
            //DEBUG_RT_PRINTF("sending stream message for chunk %d with chunkIndexinbuffer %d\n", chunkIndex + ITERATOR_ADVANCE_IN_BLOCKS, chunkIndexInBufferForStream);
            if(chunkIndexInBufferForStream == CHUNK_INVALID)
                parent->audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex + ITERATOR_ADVANCE_IN_BLOCKS);
        }
        assert(chunkIndex >= 0 && "StreamingBufferIterator::operator++ chunkIndex negative after increment");
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
                    StreamingMessage outMsg{StreamingMessageType::AssignChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer};
                    parent->audioStreamer.streamSamplesTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
                    parent->chunkStates[chunkIndexInBuffer].set(sampleIdentifier, chunkIndex, true);
                }
                if(type == SBIType::Read){
                    throw std::runtime_error("StreamingBufferIterator::operator++: stream too slow");
                }
            }
            assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent->chunks.size() && "StreamingBufferIterator::operator++ chunkIndexInBuffer out of range");
            chunkStartPtr = parent->chunks.at(chunkIndexInBuffer).data();
            data = chunkStartPtr;
            assert(chunkStartPtr != nullptr && "StreamingBufferIterator::operator++ chunkStartPtr null after reassignment");
            assert(data != nullptr && "StreamingBufferIterator::operator++ data null after reassignment");
        }
    } else {
        ++data;
        assert(data != nullptr && "StreamingBufferIterator::operator++ data null after increment");
        assert(indexInChunk < parent->chunkLength && "StreamingBufferIterator::operator++ indexInChunk exceeded chunkLength");
    }
    return *this;
}

void StreamingBufferIterator::set(SampleIdentifier sampleIdentifier,
        size_t index, StreamingBuffer* parent,
        std::vector<int>* chunkIndicesInBuffer,
        SBIType type){
    assert(parent != nullptr && "StreamingBufferIterator::set parent null");
    assert(chunkIndicesInBuffer != nullptr && "StreamingBufferIterator::set chunkIndicesInBuffer null");
    auto lengthIt = parent->availableSamples.find(sampleIdentifier);
    if(lengthIt == parent->availableSamples.end()){
        throw std::runtime_error("StreamingBufferIterator::set: sampleIdentifier missing from availableSamples");
    }
    this->sampleIdentifier = sampleIdentifier;
    this->index = index;
    this->parent = parent;
    this->chunkIndicesInBuffer = chunkIndicesInBuffer;
    this->type = type;
    this->sampleLength = lengthIt->second;
    assert(this->sampleLength > 0 && "StreamingBufferIterator::set sampleLength must be > 0");

    this->chunkIndex = index / parent->chunkLength;
    this->indexInChunk = index % parent->chunkLength;
    assert(chunkIndex != ITERATOR_INVALID && "StreamingBufferIterator::set chunkIndex invalid");
    assert(indexInChunk != ITERATOR_INVALID && "StreamingBufferIterator::set indexInChunk invalid");
    assert(indexInChunk < parent->chunkLength && "StreamingBufferIterator::set indexInChunk out of range");
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
    assert(chunkStartPtr != nullptr && "StreamingBufferIterator::set chunkStartPtr null");
    assert(data != nullptr && "StreamingBufferIterator::set data null");
    
    if(this->type == SBIType::Read){
        for(int i = 0; i <= ITERATOR_ADVANCE_IN_BLOCKS; i++){
            int chunkIndexInBufferForStream = chunkIndicesInBuffer->at(chunkIndex + i);
            //DEBUG_RT_PRINTF("sending stream message for chunk %d with chunkIndexinbuffer %d\n", chunkIndex + i, chunkIndexInBufferForStream);
            if(chunkIndexInBufferForStream == CHUNK_INVALID)
                parent->audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex + i);
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
    parent->eraseIterator(*this);
}