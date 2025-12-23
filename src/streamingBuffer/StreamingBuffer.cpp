#include <xenomai_wraps.h>
#include "../../include/streamingBuffer/StreamingBuffer.h"
#include "../../include/streamingBuffer/WavWriter.h"
#include "../../include/general/BasicUtilities.h"
//compile

///////////ChunkState/////////

void ChunkState::set(SampleIdentifier sampleIdentifier, int inChunkIndex, bool inWriteProtected, bool inChunkReady){
    ownerKey.store(sampleIdentifier.first, std::memory_order_release);
    ownerVelocity.store(sampleIdentifier.second, std::memory_order_release);
    chunkIndex.store(inChunkIndex, std::memory_order_release);
    writeProtected.store(inWriteProtected, std::memory_order_release);
    chunkReady.store(inChunkReady, std::memory_order_release);
}

///////////StreamingBuffer///////////

StreamingBuffer::StreamingBuffer(ResourceManager& resourceManager,
            size_t bufferLengthInFrames,
            int totalNumberOfIterators,
            std::string bufferName, std::string folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples)
            : resourceManager(resourceManager),
            bufferName(bufferName), folderPath(folderPath),
            chunkLength(DEFAULT_STREAMING_CHUNK_SIZE),
            totalNumberOfChunks(static_cast<int>(bufferLengthInFrames / chunkLength) + 1),
            totalNumberOfIterators(totalNumberOfIterators),
            availableSamples(availableSamples),
            iterators(std::vector<StreamingBufferIterator>(totalNumberOfIterators, StreamingBufferIterator(*this))),
            audioStreamer(*this),
            chunkStates(totalNumberOfChunks),
            chunks(totalNumberOfChunks, std::vector<float>(chunkLength)),
            mutateDataTaskName(bufferName + "_MTask"),
            mutateDataTask(this, mutateDataPrio, mutateDataTaskName)
{
    assert(bufferLengthInFrames > 0 && "StreamingBuffer ctor bufferLengthInFrames must be > 0");
    assert(totalNumberOfIterators > 0 && "StreamingBuffer ctor totalNumberOfIterators must be > 0");
    assert(chunkLength > 0 && "StreamingBuffer ctor chunkLength must be > 0");
    assert(totalNumberOfChunks > 0 && "StreamingBuffer ctor totalNumberOfChunks must be > 0");
    assert(chunks.size() == static_cast<size_t>(totalNumberOfChunks) && "StreamingBuffer ctor chunks size mismatch");
    assert(iterators.size() == static_cast<size_t>(totalNumberOfIterators) && "StreamingBuffer ctor iterators size mismatch");
    DEBUG_PRINTF("StreamingBuffer ctor(dynamic): bufferFrames=%u chunkLen=%d totalChunks=%d iters=%d name=%s path=%s\n",
        (unsigned int)bufferLengthInFrames,
        chunkLength,
        totalNumberOfChunks,
        totalNumberOfIterators,
        bufferName.c_str(),
        folderPath.c_str());
}

StreamingBuffer::~StreamingBuffer(){
    // chunkStates is now a vector, no manual deletion needed
}

void StreamingBuffer::taskWorkMessage(std::string& taskName, StreamingMessage msg){
	if(taskName == bufferName + "_MTask"){
		workMutateMessage(msg);
		return;
	}
	throw std::runtime_error("StreamingBuffer::taskWorkMessage invoked with task name " + taskName);
}

void StreamingBuffer::stopWork(){
    for(StreamingBufferIterator& it : iterators){
        it.initialize();
    }
    //clear all messages;
    StreamingMessage dummyMsg;
    while(audioStreamer.streamTask.popMessage(TaskMessageTarget::AudioThread, dummyMsg)){}
    while(audioStreamer.flushTask.popMessage(TaskMessageTarget::AudioThread, dummyMsg)){}
    while(audioStreamer.streamTask.popMessage(TaskMessageTarget::TaskThread, dummyMsg)){}
    while(audioStreamer.flushTask.popMessage(TaskMessageTarget::TaskThread, dummyMsg)){}
    //wait till all ongoing work is done. (a task might still work on one message, all the others have been popped. 1e8ns is plenty.)
    task_sleep_ns(1e8);
}

void StreamingBuffer::workMutateMessage(StreamingMessage msg){
    if(msg.type == StreamingMessageType::releaseMutateOngoing){
        mutateOngoing.store(false, std::memory_order_acquire);
    }
    if(msg.type == StreamingMessageType::InitializeSample){
        auto insertResult = chunkIndicesInBufferMap.insert(std::make_pair(msg.sampleIdentifier, std::vector<int>()));
        std::vector<int>& chunkIndicesInBuffer = insertResult.first->second;
        //If I initialize a single sample on the fly i have to free all the chunkStates associated with it.
        if(!insertResult.second){
            for(int chunkIndexInBuffer: chunkIndicesInBuffer){
                if(chunkIndexInBuffer != CHUNK_INVALID){
                    assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < totalNumberOfChunks && "StreamingBuffer::workMutateMessage reset chunkIndexInBuffer out of range");
                    VEC_AT(chunkStates, chunkIndexInBuffer).set({CHUNKSTATE_INVALID, CHUNKSTATE_INVALID}, CHUNKSTATE_INVALID, false, false);
                }
            }
        }
        assert(msg.chunkIndex > 0);
        chunkIndicesInBufferMap[msg.sampleIdentifier] = std::vector<int>(msg.chunkIndex, CHUNK_INVALID);
    }
    if(msg.type == StreamingMessageType::Clear){
        chunkIndicesInBufferMap.clear();
        audioStreamer.pendingFlushes.clear();
        audioStreamer.f_numberOfFlushableChunks.clear();
        for(size_t chunkStateIndex = 0; chunkStateIndex < static_cast<size_t>(totalNumberOfChunks); chunkStateIndex++){
            VEC_AT(chunkStates, chunkStateIndex).set({CHUNKSTATE_INVALID, CHUNKSTATE_INVALID}, CHUNKSTATE_INVALID, false, false);
        }
    }
}

//TODO: be careful with extremely high playbackrates larger than 20 or so there are problems.
StreamingBufferIterator& StreamingBuffer::begin(SampleIdentifier sampleIdentifier, SBIType type, float playbackRate){
    assert(sampleIdentifier.first >= 0 && sampleIdentifier.first < 128 && "StreamingBuffer::begin key out of MIDI range");
    assert(sampleIdentifier.second >= 0 && sampleIdentifier.second < 128 && "StreamingBuffer::begin velocity out of MIDI range");

    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    
    //find iterator to use
    for(int i = 0; i < totalNumberOfIterators; i++){
        StreamingBufferIteratorAssignIndex = (StreamingBufferIteratorAssignIndex + 1) % totalNumberOfIterators;
        if(VEC_AT(iterators, StreamingBufferIteratorAssignIndex).sampleIdentifier.first == ITERATOR_INVALID){
            break;
        }
        if(i == totalNumberOfIterators - 1){
            throw std::runtime_error("StreamingBuffer::begin: no free iterator slots available.\n");
        }
    }

    //see if first chunk is valid
    int chunkIndexInBuffer = VEC_AT(chunkIndicesInBuffer, 0);
    assert(chunkIndexInBuffer > 0 && chunkIndexInBuffer < totalNumberOfChunks);
    ChunkState& chunkState = VEC_AT(chunkStates, chunkIndexInBuffer);
    if(type == SBIType::Read){
        if(chunkIndexInBuffer == CHUNK_INVALID) throw std::runtime_error("apparently the stream chunk message was never sent");
        if(!chunkState.chunkReady.load(std::memory_order_acquire)) throw std::runtime_error("first chunk should always be ready for read iterator");
    }
    if(type == SBIType::Write){
        if(chunkIndexInBuffer == CHUNK_INVALID) chunkIndexInBuffer = assignToFreeChunk(sampleIdentifier, 0, chunkIndicesInBuffer);
    }

    protectSample(sampleIdentifier);

    int streamingAdvanceInChunks = std::ceil(playbackRate) * DEFAULT_STREAMING_ADVANCE_IN_CHUNKS;
    StreamingBufferIterator& sbi = VEC_AT(iterators, StreamingBufferIteratorAssignIndex);
    sbi.set(sampleIdentifier, &chunkIndicesInBuffer, type, streamingAdvanceInChunks);

    return sbi;
}

void StreamingBuffer::sendStreamStartsMessages(){
    for(auto& sampleInfo: availableSamples){
        SampleIdentifier sampleIdentifier = sampleInfo.first;
        std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
        if(VEC_AT(chunkIndicesInBuffer, 0) != CHUNK_INVALID)
            throw std::runtime_error("StreamingBuffer::sendStreamStartsMessages should only be used upon initialization, where every start chunk is invalid");
        audioStreamer.sendStreamChunkMessage(sampleIdentifier, 0, chunkIndicesInBuffer);
    }
}

int StreamingBuffer::sampleInUse(SampleIdentifier sampleIdentifier){

    bool iteratorFound = std::any_of(iterators.begin(),iterators.end(),
        [&](const StreamingBufferIterator& iter) {
            return iter.sampleIdentifier == sampleIdentifier;
        }
    );

    bool flushFound = std::any_of(audioStreamer.pendingFlushes.begin(),audioStreamer.pendingFlushes.end(),
        [&](const SampleIdentifier& sId) {
            return sId == sampleIdentifier;
        }
    );

    if(flushFound && iteratorFound) return BOTH_EXIST;
    if(flushFound) return FLUSH_EXISTS;
    if(iteratorFound) return IT_EXISTS;
    return NOT_IN_USE;
}

void StreamingBuffer::releaseIterator(StreamingBufferIterator& iterator){
    auto it = std::find_if(iterators.begin(), iterators.end(),
        [&iterator](const StreamingBufferIterator& elem) {
            return &elem == &iterator;
        }); //check if adress of elem is equal to address of iterator
    if (it == iterators.end()) {
        throw std::runtime_error("StreamingBuffer::releaseIterator: iterator not found in iterators vector\n");
    }
    StreamingBufferIterator& itRef = *it;
    if(itRef.type == SBIType::Write){
        audioStreamer.sendFlushChunksMessages(itRef.sampleIdentifier);
    }
    SampleIdentifier oldSampleIdentifier = itRef.sampleIdentifier;
    itRef.initialize();
    if(!sampleInUse(oldSampleIdentifier)) unProtectSample(oldSampleIdentifier);
}

void StreamingBuffer::processBlockwise(){
    mutateDataTask.taskCheckAndWorkMessages();
    //TODO: barrier if mutate task in flight?
    audioStreamer.processBlockwise();
}

void StreamingBuffer::streamFullSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    int numberOfChunks = chunkIndicesInBuffer.size();
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        int chunkIndexInBuffer = VEC_AT(chunkIndicesInBuffer, chunkIndex);
        if(chunkIndexInBuffer == CHUNK_INVALID){
            audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex, chunkIndicesInBuffer);
        }else{
            assert(chunkIndexInBuffer > 0 && chunkIndexInBuffer < totalNumberOfChunks);
            if(!VEC_AT(chunkStates, chunkIndexInBuffer).chunkReady.load(std::memory_order_acquire))
                audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex, chunkIndicesInBuffer);
        }
    }
}

void StreamingBuffer::protectSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    for(int chunkIndex = 0; chunkIndex < chunkIndicesInBuffer.size(); chunkIndex++){
        int chunkIndexInBuffer = VEC_AT(chunkIndicesInBuffer, chunkIndex);
        if(chunkIndexInBuffer == CHUNK_INVALID) continue;
        VEC_AT(chunkStates, chunkIndexInBuffer).writeProtected.store(true, std::memory_order_release);
    }
}

void StreamingBuffer::unProtectSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    for(int chunkIndex = 0; chunkIndex < chunkIndicesInBuffer.size(); chunkIndex++){
        int chunkIndexInBuffer = VEC_AT(chunkIndicesInBuffer, chunkIndex);
        if(chunkIndexInBuffer == CHUNK_INVALID) continue;
        VEC_AT(chunkStates, chunkIndexInBuffer).writeProtected.store(false, std::memory_order_release);
    }
}

void StreamingBuffer::sendInitializeSamplesMessages(std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    assert(!availableSamples.empty() && "StreamingBuffer::initializeSamples called with no samples");
    
    for(auto& sampleInfo: availableSamples){
        sendInitializeSampleMessage(sampleInfo.first, sampleInfo.second);
    }
}

void StreamingBuffer::sendInitializeSampleMessage(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames){
    assert(expectedSampleLengthInFrames > 0 && "StreamingBuffer::initializeSample expectedSampleLengthInFrames must be > 0");
    int numberOfChunks = static_cast<int>(expectedSampleLengthInFrames / chunkLength) + 1;
    StreamingMessage outMsg{StreamingMessageType::InitializeSample, sampleIdentifier, numberOfChunks, ARBITRARY_VALUE};
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
}

void StreamingBuffer::clear(){
    //lock other threads from working. TODO for the future: make this a mutex lock instead of atomic bool.
    mutateOngoing.store(true, std::memory_order_acquire);
    //first pop all remaining messages and wait for the ongoing taskWorkMessages to finish.
    this->stopWork();
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, {StreamingMessageType::Clear, {ARBITRARY_VALUE, ARBITRARY_VALUE}, ARBITRARY_VALUE, ARBITRARY_VALUE});
    mutateDataTask.taskCheckAndWorkMessages();
}

//!!This should be run by an external non-Audio thread
void StreamingBuffer::initializeForNewSamplePack(std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    sendInitializeSamplesMessages(availableSamples);
    //mutateDataTask.taskCheckAndWorkMessages();
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, {StreamingMessageType::releaseMutateOngoing, {ARBITRARY_VALUE,ARBITRARY_VALUE}, ARBITRARY_VALUE, ARBITRARY_VALUE});
    mutateDataTask.taskCheckAndWorkMessages();
    //TODO for the future. experiment around with that waittime. is it necessary and how much?
    task_sleep_ns(1e8);
    sendStreamStartsMessages();
    audioStreamer.streamTask.taskCheckAndWorkMessages();
}

void StreamingBuffer::printInfo(){
    rt_printf("StreamingBuffer info:\n");
    rt_printf("  name=%s path=%s\n", bufferName.c_str(), folderPath.c_str());
    rt_printf("  chunkLength=%d totalChunks=%d iterators=%d\n", chunkLength, totalNumberOfChunks, totalNumberOfIterators);
    rt_printf("  chunks size=%zu chunkStates=%zu\n", chunks.size(), chunkStates.size());
    rt_printf("  availableSamples=%zu chunkIndicesInBufferMap=%zu\n", availableSamples.size(), chunkIndicesInBufferMap.size());
    int counter = 0;
    for(auto& availableSample: availableSamples){
        counter++;
        if(counter > 4) break;
        SampleIdentifier sampleIdentifier = availableSample.first;
        rt_printf("chunkIndicesInBufferMap[%d,%d] = {%d, %d, ...\n", sampleIdentifier.first, sampleIdentifier.second, VEC_AT(chunkIndicesInBufferMap[sampleIdentifier], 0), VEC_AT(chunkIndicesInBufferMap[sampleIdentifier], 1));

    }
    rt_printf("...\n");
    rt_printf("  iteratorAssignIndex=%zu freeChunkIdx=%zu\n", StreamingBufferIteratorAssignIndex, freeChunkSearchIdx.load(std::memory_order_relaxed));
}

void StreamingBuffer::printIterators(){
    int index = 0;
    for(auto& iterator: iterators){
        if(iterator.sampleIdentifier.first == ITERATOR_INVALID){
            rt_printf("iterator %d: free\n", index);
        } else {
            rt_printf("iterator %d: sampleId: %d_%d\n", index, iterator.sampleIdentifier.first, iterator.sampleIdentifier.second);
        }
        index++;
    }
}

int StreamingBuffer::findFreeChunk(){
    // Start from a rotating index to avoid scanning from 0 each time.
    size_t start = freeChunkSearchIdx.fetch_add(1, std::memory_order_acq_rel) % totalNumberOfChunks;
    for(size_t offset = 0; offset < totalNumberOfChunks; ++offset){
        size_t chunkIndexInBuffer = (start + offset) % totalNumberOfChunks;
        int chunkIndex = VEC_AT(chunkStates, chunkIndexInBuffer).chunkIndex.load(std::memory_order_acquire);
        if(chunkIndex != 0){ //NEVER OVERWRITE A FIRST CHUNK
            if(!( VEC_AT(chunkStates, chunkIndexInBuffer).writeProtected.load(std::memory_order_acquire) )){
                return static_cast<int>(chunkIndexInBuffer);
            }
        }
    }
    throw std::runtime_error("did not find a free chunk\n");
    return ARBITRARY_VALUE;
}

int StreamingBuffer::assignToFreeChunk(SampleIdentifier sampleIdentifier, int chunkIndex, std::vector<int>& chunkIndicesInBuffer){
    int chunkIndexInBuffer = findFreeChunk();
    VEC_AT(chunkIndicesInBuffer, chunkIndex) = chunkIndexInBuffer;

    //update ownership of the chunk
    ChunkState& chunkState = VEC_AT(chunkStates, chunkIndexInBuffer);
    SampleIdentifier oldSampleIdentifier = {chunkState.ownerKey.load(std::memory_order_acquire), 
                                            chunkState.ownerVelocity.load(std::memory_order_acquire)};
    int oldChunkIndex = chunkState.chunkIndex.load(std::memory_order_acquire);
    chunkState.set(sampleIdentifier, chunkIndex, true, false);

    //release old owner
    if(oldSampleIdentifier.first != CHUNKSTATE_INVALID){
        std::vector<int>& oldChunkIndicesInBuffer = getChunkIndicesInBuffer(oldSampleIdentifier);
        VEC_AT(oldChunkIndicesInBuffer, oldChunkIndex) = CHUNK_INVALID;
    }
    return chunkIndexInBuffer;
}

size_t StreamingBuffer::getSampleLength(SampleIdentifier sampleIdentifier){
    auto lengthIt = availableSamples.find(sampleIdentifier);
    if(lengthIt == availableSamples.end()){
        throw std::runtime_error("StreamingBuffer::getSampleLength: sampleIdentifier not in availableSamples");
    }
    const size_t sampleLength = lengthIt->second;
    if(sampleLength == 0){
        //TODO: make possible to set length on the fly
        throw std::runtime_error("StreamingBuffer::getSampleLength: sample length is zero");
    }
    return sampleLength;
}

std::vector<int>& StreamingBuffer::getChunkIndicesInBuffer(SampleIdentifier sampleIdentifier){
    auto mapIt = chunkIndicesInBufferMap.find(sampleIdentifier);
    if(mapIt == chunkIndicesInBufferMap.end()){
        throw std::runtime_error(" StreamingBuffer::getChunkIndicesInBuffer: chunkIndicesInBufferMap missing sample");
    }
    std::vector<int>& chunkIndicesInBuffer = mapIt->second;
    if(chunkIndicesInBuffer.size() <= 0){
        throw std::runtime_error(" StreamingBuffer::getChunkIndicesInBuffer: chunkIndicesInBuffer length is <=0");
    }
    return chunkIndicesInBuffer;
}
