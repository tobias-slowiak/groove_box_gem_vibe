#include "../../include/streamingBuffer/StreamingBuffer.h"
#include "../../include/general/BasicUtilities.h"
#include <cmath>


///////////ElementProxy///////////

ElementProxy& ElementProxy::operator=(float value){
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        throw std::runtime_error("writing to invalid Iterator");
    }
    assert(iterator.data != nullptr && "ElementProxy::operator= data null");
    if(value == END_OF_SAMPLE){
        iterator.sampleLength = iterator.parent.getSampleLength(iterator.sampleIdentifier);
        printf("Writing END_OF_SAMPLE at index %zu of sample %d_%d with length %zu\n", iterator.index, iterator.sampleIdentifier.first, iterator.sampleIdentifier.second, iterator.sampleLength);
        if(iterator.sampleLength != iterator.index){
            std::string errMsg = "END_OF_SAMPLE can only be written at the end of the sample. Current index: " + std::to_string(iterator.index) + ", sample length: " + std::to_string(iterator.sampleLength);
            throw std::runtime_error(errMsg);
        }
        iterator.markChunkAsReady(); //mark last chunk as ready
    }
    *(iterator.data) = value;
    return *this;
}


ElementProxy::operator float() const {
    if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
        return END_OF_SAMPLE;
    }
    assert(iterator.data != nullptr && "ElementProxy::operator float data null");
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
        chunkIndicesInBuffer(&EMPTY_CHUNK_INDEX_VECTOR),
        type(SBIType::None),
        streamingAdvanceInChunks(DEFAULT_STREAMING_ADVANCE_IN_CHUNKS),
        sampleLength(0),
        chunkIndex(ITERATOR_INVALID),
        indexInChunk(ITERATOR_INVALID),
        chunkIndexInBuffer(ITERATOR_INVALID),
        chunkStartPtr(nullptr),
        data(nullptr){
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
    if(index >= sampleLength){
        if(sampleLength % parent.chunkLength != 0){
            data++;
            if(*data != END_OF_SAMPLE){
                float second_last = *(--data);
                float last = *(++data);
                float after_last = 0.0f;
                assert(++data != nullptr);
                after_last = *data;
                std::string errMsg = "iterator at end of sample but entry is not END_OF_SAMPLE, last entries are: (middle one should be ENDOFSAMLE): ["
                                    + std::to_string(second_last) + ", " + std::to_string(last) + ", " + std::to_string(after_last)
                                    + "] for sample " + std::to_string(sampleIdentifier.first) + "_" + std::to_string(sampleIdentifier.second)
                                    + " with info: index: " + std::to_string(index) + ", sampleLength: " + std::to_string(sampleLength);
                throw std::runtime_error(errMsg);
            }
        }
        if(type == SBIType::Read){
            this->release(); //TODO: do i really always want to release on end of sample for read iterators?
        }
        return *this;
    }
    indexInChunk++;
    if(indexInChunk >= parent.chunkLength){
        ++chunkIndex;
        indexInChunk = 0;
        ChunkState& oldChunkState = VEC_AT(parent.chunkStates, chunkIndexInBuffer);
        chunkIndexInBuffer = VEC_AT(*chunkIndicesInBuffer, chunkIndex);

        if(type == SBIType::Read){
            if(chunkIndex + streamingAdvanceInChunks <= sampleLength / parent.chunkLength){
                int chunkIndexInBufferForStream = VEC_AT(*chunkIndicesInBuffer, chunkIndex + streamingAdvanceInChunks);
                if(chunkIndexInBufferForStream == CHUNK_INVALID)
                    parent.audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex + streamingAdvanceInChunks, *chunkIndicesInBuffer);
            }
            if(chunkIndexInBuffer == CHUNK_INVALID) throw std::runtime_error("apparently the stream chunk message was never sent");
            ChunkState& chunkState = VEC_AT(parent.chunkStates, chunkIndexInBuffer);
            if(!chunkState.chunkReady.load(std::memory_order_acquire)) throw std::runtime_error("stream too slow for "+ std::to_string(sampleIdentifier.first) + "_" + std::to_string(sampleIdentifier.second) + " and chunk " + std::to_string(chunkIndex)+ " with total number of chunks " + std::to_string(std::ceil((float)sampleLength / parent.chunkLength)));
        }
        if(type == SBIType::Write){
            oldChunkState.chunkReady.store(true, std::memory_order_release); 
            if(chunkIndexInBuffer == CHUNK_INVALID) chunkIndexInBuffer = parent.assignToFreeChunk(sampleIdentifier, chunkIndex, *chunkIndicesInBuffer);            
        }

        chunkStartPtr = VEC_AT(parent.chunks, chunkIndexInBuffer).data();
        data = chunkStartPtr;
    } else {
        ++data;
    }
    assert(data != nullptr && "StreamingBufferIterator::operator++ data null after reassignment");
    return *this;
}

void StreamingBufferIterator::set(SampleIdentifier sampleIdentifier,
                                std::vector<int>* chunkIndicesInBuffer,
                                SBIType type,
                                int streamingAdvanceInChunks){
    this->sampleLength = parent.getSampleLength(sampleIdentifier);
    this->sampleIdentifier = sampleIdentifier;
    this->index = 0;
    this->chunkIndicesInBuffer = chunkIndicesInBuffer;
    this->type = type;
    assert(this->sampleLength > 0 && "StreamingBufferIterator::set sampleLength must be > 0");

    this->chunkIndex = 0;
    this->indexInChunk = 0;
    assert(chunkIndicesInBuffer != nullptr  && chunkIndicesInBuffer->size() != 0 && "StreamingBufferIterator::set chunkIndicesInBuffer null");
    this->chunkIndexInBuffer = VEC_AT(*chunkIndicesInBuffer, 0);
    this->chunkStartPtr = VEC_AT(parent.chunks, chunkIndexInBuffer).data();
    this->data = chunkStartPtr;
    
    if(this->type == SBIType::Read){
        for(int i = 0; i <= streamingAdvanceInChunks; i++){
            if(chunkIndex + i >= chunkIndicesInBuffer->size())
                break;
            int chunkIndexInBufferForStream = VEC_AT(*chunkIndicesInBuffer, chunkIndex + i);
            if(chunkIndexInBufferForStream == CHUNK_INVALID)
                parent.audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex + i, *chunkIndicesInBuffer);
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

void StreamingBufferIterator::flush(){
    parent.flushSample(sampleIdentifier);
}

void StreamingBufferIterator::release(){
    parent.releaseIterator(*this);
}

void StreamingBufferIterator::rewind(){
    if(sampleIdentifier.first == ITERATOR_INVALID){
        throw std::runtime_error("using rewind on invalid iterator");
    }
    index = 0;
    chunkIndex = 0;
    indexInChunk = 0;
    chunkIndexInBuffer = VEC_AT(*chunkIndicesInBuffer, 0);
    chunkStartPtr = VEC_AT(parent.chunks, chunkIndexInBuffer).data();
    data = chunkStartPtr;
}

void StreamingBufferIterator::markChunkAsReady(){
    ChunkState& oldChunkState = VEC_AT(parent.chunkStates, chunkIndexInBuffer);
    oldChunkState.chunkReady.store(true, std::memory_order_release); 
}