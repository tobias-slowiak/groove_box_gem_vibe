#include <xenomai_wraps.h>
#include "../include/StreamingBuffer.h"
#include "../include/WavWriter.h"

///////////ChunkState/////////

void ChunkState::set(SampleIdentifier sampleIdentifier, int inChunkIndex, bool inWriteProtected, bool inChunkReady){
    ownerKey.store(sampleIdentifier.first, std::memory_order_release);
    ownerVelocity.store(sampleIdentifier.second, std::memory_order_release);
    chunkIndex.store(inChunkIndex, std::memory_order_release);
    writeProtected.store(inWriteProtected, std::memory_order_release);
    chunkReady.store(inChunkReady, std::memory_order_release);
}

///////////StreamingBuffer///////////

StreamingBuffer::StreamingBuffer(ResourceManager* resourceManager,
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
            iterators(std::vector<StreamingBufferIterator>(totalNumberOfIterators, StreamingBufferIterator(this))),
            audioStreamer(*this),
            chunkStates(new ChunkState[totalNumberOfChunks]),
            chunks(totalNumberOfChunks, std::vector<float>(chunkLength)),
            mutateDataTaskName(bufferName + "_MTask"),
            mutateDataTask(this, mutateDataPrio, mutateDataTaskName)
{
    assert(resourceManager != nullptr && "StreamingBuffer ctor resourceManager null");
    assert(bufferLengthInFrames > 0 && "StreamingBuffer ctor bufferLengthInFrames must be > 0");
    assert(totalNumberOfIterators > 0 && "StreamingBuffer ctor totalNumberOfIterators must be > 0");
    assert(chunkLength > 0 && "StreamingBuffer ctor chunkLength must be > 0");
    assert(totalNumberOfChunks > 0 && "StreamingBuffer ctor totalNumberOfChunks must be > 0");
    assert(chunkStates != nullptr && "StreamingBuffer ctor chunkStates allocation failed");
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
    delete[] chunkStates;
}

void StreamingBuffer::taskWorkMessage(std::string& taskName, StreamingMessage msg){
	if(taskName == bufferName + "_MTask"){
		workMutateMessage(msg);
		return;
	}
	throw std::runtime_error("StreamingBuffer::taskWorkMessage invoked with task name " + taskName);
}

void StreamingBuffer::stopWork(){
    //stop all iterators
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
    assert(chunkStates != nullptr && "StreamingBuffer::workMutateMessage chunkStates null");
    if(msg.type == StreamingMessageType::releaseMutateOngoing){
        mutateOngoing.store(false, std::memory_order_acquire);
    }
    if(msg.type == StreamingMessageType::InitializeSample){
        assert(msg.chunkIndex > 0 && "StreamingBuffer::mutate InitializeSample with non-positive chunkIndex");
        auto insertResult = chunkIndicesInBufferMap.insert(std::make_pair(msg.sampleIdentifier, std::vector<int>()));
        std::vector<int>& chunkIndicesInBuffer = insertResult.first->second;
        //If I initialize a single sample on the fly i have to free all the chunkStates associated with it.
        if(!insertResult.second){
            for(int chunkIndexInBuffer: chunkIndicesInBuffer){
                if(chunkIndexInBuffer != CHUNK_INVALID){
                    assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < chunks.size() && "StreamingBuffer::workMutateMessage reset chunkIndexInBuffer out of range");
                    chunkStates[chunkIndexInBuffer].set({CHUNKSTATE_INVALID, CHUNKSTATE_INVALID}, CHUNKSTATE_INVALID, false, false);
                }
            }
        }
        chunkIndicesInBufferMap[msg.sampleIdentifier] = std::vector<int>(msg.chunkIndex, CHUNK_INVALID);
    }
    if(msg.type == StreamingMessageType::Clear){
        DEBUG_PRINTF("StreamingBuffer::workMutateMessage Clear\n");
        chunkIndicesInBufferMap.clear();
        audioStreamer.pendingFlushes.clear();
        audioStreamer.f_numberOfFlushableChunks.clear();
        for(size_t chunkStateIndex = 0; chunkStateIndex < static_cast<size_t>(totalNumberOfChunks); chunkStateIndex++){
            chunkStates[chunkStateIndex].set({CHUNKSTATE_INVALID, CHUNKSTATE_INVALID}, CHUNKSTATE_INVALID, false, false);
        }
    }
}

//TODO: be careful with extremely high playbackrates larger than 20 or so there are problems.
StreamingBufferIterator& StreamingBuffer::begin(SampleIdentifier sampleIdentifier, SBIType type, float playbackRate){
    assert(sampleIdentifier.first >= 0 && sampleIdentifier.first < 128 && "StreamingBuffer::begin key out of MIDI range");
    assert(sampleIdentifier.second >= 0 && sampleIdentifier.second < 128 && "StreamingBuffer::begin velocity out of MIDI range");

    //find if sample and chunkindicesinbuffer is available.
    auto lengthIt = availableSamples.find(sampleIdentifier);
    if(lengthIt == availableSamples.end()){
        throw std::runtime_error("StreamingBuffer::begin: sampleIdentifier not in availableSamples");
    }
    const size_t sampleLength = lengthIt->second;
    if(sampleLength == 0){
        //TODO: make possible to set length on the fly
        throw std::runtime_error("StreamingBuffer::begin: sample length is zero");
    }
    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    
    //find iterator to use
    for(int i = 0; i < totalNumberOfIterators; i++){
        StreamingBufferIteratorAssignIndex = (StreamingBufferIteratorAssignIndex + 1) % totalNumberOfIterators;
        assert(StreamingBufferIteratorAssignIndex < iterators.size() && "StreamingBuffer::begin iterator index out of range");
        if(iterators.at(StreamingBufferIteratorAssignIndex).sampleIdentifier.first == ITERATOR_INVALID){
            break;
        }
        if(i == totalNumberOfIterators - 1){
            throw std::runtime_error("StreamingBuffer::begin: no free iterator slots available.\n");
        }
    }

    //see if first chunk is valid
    assert(0 < chunkIndicesInBuffer.size() );
    int chunkIndexInBuffer = chunkIndicesInBuffer.at(0);
    assert(chunkIndexInBuffer < totalNumberOfChunks);
    ChunkState& chunkState = chunkStates[chunkIndexInBuffer];
    if(type == SBIType::Read){
        if(chunkIndexInBuffer == CHUNK_INVALID) throw std::runtime_error("apparently the stream chunk message was never sent");
        if(!chunkState.chunkReady.load(std::memory_order_acquire)) throw std::runtime_error("first chunk should always be ready for read iterator");
    }
    if(type == SBIType::Write){
        if(chunkIndexInBuffer == CHUNK_INVALID) chunkIndexInBuffer = assignToFreeChunk(sampleIdentifier, 0, chunkIndicesInBuffer);
    }

    protectSample(sampleIdentifier);

    int streamingAdvanceInChunks = std::ceil(playbackRate) * DEFAULT_STREAMING_ADVANCE_IN_CHUNKS;
    iterators.at(StreamingBufferIteratorAssignIndex).set(sampleIdentifier, &chunkIndicesInBuffer, type, streamingAdvanceInChunks);

    return iterators.at(StreamingBufferIteratorAssignIndex);
}

void StreamingBuffer::sendStreamStartsMessages(){
    assert(chunkStates != nullptr && "StreamingBuffer::streamStarts chunkStates null");
    for(auto& sampleInfo: availableSamples){
        SampleIdentifier sampleIdentifier = sampleInfo.first;
        std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
        assert(!chunkIndicesInBuffer.empty() && "AudioStreamer::sendStreamStartsMessage empty chunkIndicesInBuffer");
        if(chunkIndicesInBuffer.at(0) != CHUNK_INVALID)
            throw std::runtime_error("StreamingBuffer::sendStreamStartsMessages should only be used upon initialization, where every start chunk is invalid");
        audioStreamer.sendStreamChunkMessage(sampleIdentifier, 0, chunkIndicesInBuffer);
    }
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

}

void StreamingBuffer::audioCheckAndWorkMessages(){
    audioStreamer.audioCheckAndWorkMessages();
    //no messages from the M-Task expected right now.
}

void StreamingBuffer::processBlockwise(){
    mutateDataTask.taskCheckAndWorkMessages();
    //TODO: barrier if mutate task in flight?
    audioStreamer.processBlockwise();
}

void StreamingBuffer::streamFullSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    assert(!chunkIndicesInBuffer.empty() && "StreamingBuffer::streamFullSample empty chunkIndicesInBuffer");
    int numberOfChunks = chunkIndicesInBuffer.size();
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "AudioStreamer::streamFromDisk chunkIndex out of range");
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        if(chunkIndexInBuffer == CHUNK_INVALID){
            audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex, chunkIndicesInBuffer);
        }else{
            assert(chunkIndexInBuffer < totalNumberOfChunks);
            if(!chunkStates[chunkIndexInBuffer].chunkReady.load(std::memory_order_acquire))
                audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex, chunkIndicesInBuffer);
        }
    }
}

void StreamingBuffer::protectSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    assert(!chunkIndicesInBuffer.empty() && "protectSample chunkIndicesInBuffer empty");
    for(int chunkIndex = 0; chunkIndex < chunkIndicesInBuffer.size(); chunkIndex++){
        assert(chunkIndicesInBuffer.size() > chunkIndex && "protectsample: chunkindicesinbuffer to small");
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "protectsample: at(chunkIndex) out of range");
        if(chunkIndicesInBuffer.at(chunkIndex) == CHUNK_INVALID){
            continue;
        }
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < chunks.size() && "protectsample: chunkIndexInBuffer out of range");
        chunkStates[chunkIndexInBuffer].writeProtected.store(true, std::memory_order_release);
    }
}

void StreamingBuffer::unProtectSample(SampleIdentifier sampleIdentifier){
    std::vector<int>& chunkIndicesInBuffer = getChunkIndicesInBuffer(sampleIdentifier);
    assert(!chunkIndicesInBuffer.empty() && "unProtectSample chunkIndicesInBuffer empty");
    for(int chunkIndex = 0; chunkIndex < chunkIndicesInBuffer.size(); chunkIndex++){
        assert(chunkIndicesInBuffer.size() > chunkIndex && "unprotectsample: chunkindicesinbuffer to small");
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "unprotectsample: at(chunkIndex) out of range");
        if(chunkIndicesInBuffer.at(chunkIndex) == CHUNK_INVALID){
            continue;
        }
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < chunks.size() && "unprotectsample: chunkIndexInBuffer out of range");
        chunkStates[chunkIndexInBuffer].writeProtected.store(false, std::memory_order_release);
    }
}

void StreamingBuffer::sendInitializeSamplesMessages(std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    //DEBUG_PRINTF("StreamingBuffer::initializeSamples count=%zu\n", availableSamples.size());
    assert(!availableSamples.empty() && "StreamingBuffer::initializeSamples called with no samples");
    
    for(auto& sampleInfo: availableSamples){
        sendInitializeSampleMessage(sampleInfo.first, sampleInfo.second);
    }
}

void StreamingBuffer::sendInitializeSampleMessage(SampleIdentifier sampleIdentifier, size_t expectedSampleLengthInFrames){
    //DEBUG_PRINTF("StreamingBuffer::initializeSample %d_%d frames=%u\n",
    //    sampleIdentifier.first, sampleIdentifier.second, (unsigned int)expectedSampleLengthInFrames);
    assert(expectedSampleLengthInFrames > 0 && "StreamingBuffer::initializeSample expectedSampleLengthInFrames must be > 0");
    int numberOfChunks = static_cast<int>(expectedSampleLengthInFrames / chunkLength) + 1;
    StreamingMessage outMsg{StreamingMessageType::InitializeSample, sampleIdentifier, numberOfChunks, ARBITRARY_VALUE};
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
}

void StreamingBuffer::clear(){
    //lock other threads from working
    mutateOngoing.store(true, std::memory_order_acquire);
    //first pop all remaining messages and wait for the ongoing taskWorkMessages to finish.
    this->stopWork();
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, {StreamingMessageType::Clear, {ARBITRARY_VALUE, ARBITRARY_VALUE}, ARBITRARY_VALUE, ARBITRARY_VALUE});
    mutateDataTask.taskCheckAndWorkMessages();
}

void StreamingBuffer::initializeForNewSamplePack(std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    //should be run by an external non-Audio thread
    sendInitializeSamplesMessages(availableSamples);
    //mutateDataTask.taskCheckAndWorkMessages();
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, {StreamingMessageType::releaseMutateOngoing, {ARBITRARY_VALUE,ARBITRARY_VALUE}, ARBITRARY_VALUE, ARBITRARY_VALUE});
    mutateDataTask.taskCheckAndWorkMessages();
    task_sleep_ns(1e8);
    sendStreamStartsMessages();
    audioStreamer.streamTask.taskCheckAndWorkMessages();
    //now loads the starts and sends back info about the chunks to the audio thread
    task_sleep_ns(1e8);
    audioCheckAndWorkMessages();
}

void StreamingBuffer::printInfo(){
    rt_printf("StreamingBuffer info:\n");
    rt_printf("  name=%s path=%s\n", bufferName.c_str(), folderPath.c_str());
    rt_printf("  chunkLength=%d totalChunks=%d iterators=%d\n", chunkLength, totalNumberOfChunks, totalNumberOfIterators);
    rt_printf("  chunks size=%zu chunkStates=%s\n", chunks.size(), chunkStates ? "yes" : "no");
    rt_printf("  availableSamples=%zu chunkIndicesInBufferMap=%zu\n", availableSamples.size(), chunkIndicesInBufferMap.size());
    int counter = 0;
    for(auto& availableSample: availableSamples){
        counter++;
        if(counter > 4) break;
        SampleIdentifier sampleIdentifier = availableSample.first;
        rt_printf("chunkIndicesInBufferMap[%d,%d] = {%d, %d, ...\n", sampleIdentifier.first, sampleIdentifier.second, chunkIndicesInBufferMap[sampleIdentifier].at(0), chunkIndicesInBufferMap[sampleIdentifier].at(1));

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
    assert(chunkStates != nullptr && "findFreeChunk chunkStates null");
    assert(chunks.size() == static_cast<size_t>(totalNumberOfChunks) && "findFreeChunk chunks size mismatch");
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
    return ARBITRARY_VALUE;
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

int StreamingBuffer::assignToFreeChunk(SampleIdentifier sampleIdentifier, int chunkIndex, std::vector<int>& chunkIndicesInBuffer){
    int chunkIndexInBuffer = findFreeChunk();
    assert(chunkIndex < chunkIndicesInBuffer.size());
    chunkIndicesInBuffer.at(chunkIndex) = chunkIndexInBuffer;

    //update ownership of the chunk
    assert(chunkStates != nullptr && chunkIndexInBuffer < totalNumberOfChunks && "StreamingBuffer::begin chunkStates null");
    ChunkState& chunkState = chunkStates[chunkIndexInBuffer];
    SampleIdentifier oldSampleIdentifier = {chunkState.ownerKey.load(std::memory_order_acquire), 
                                            chunkState.ownerVelocity.load(std::memory_order_acquire)};
    int oldChunkIndex = chunkState.chunkIndex.load(std::memory_order_acquire);
    chunkState.set(sampleIdentifier, chunkIndex, true, false);

    //release old owner
    if(oldSampleIdentifier.first != CHUNKSTATE_INVALID){
        //TODO: sicherer map access?
        std::vector<int>& oldChunkIndicesInBuffer = chunkIndicesInBufferMap[oldSampleIdentifier];
        assert(oldChunkIndex < oldChunkIndicesInBuffer.size());
        oldChunkIndicesInBuffer.at(oldChunkIndex) = CHUNK_INVALID;
    }
    return chunkIndexInBuffer;
}

