#include <xenomai_wraps.h>
#include "../include/StreamingBuffer.h"
#include "../include/WavWriter.h"
//compile

///////////ChunkState/////////

void ChunkState::set(SampleIdentifier sampleIdentifier, int inChunkIndex, bool inWriteProtected){
    ownerKey.store(sampleIdentifier.first, std::memory_order_release);
    ownerVelocity.store(sampleIdentifier.second, std::memory_order_release);
    chunkIndex.store(inChunkIndex, std::memory_order_release);
    writeProtected.store(inWriteProtected, std::memory_order_release);
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

void StreamingBuffer::taskWorkMessage(std::string& taskName, StreamingMessage msg){
	if(taskName == bufferName + "_MTask"){
		mutate(msg);
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
    while(audioStreamer.streamSamplesTask.popMessage(TaskMessageTarget::AudioThread, dummyMsg)){
        DEBUG_PRINTF("StreamingBuffer::mutate popping msg to empty with sampleId: %d %d and chunkIndex %d\n", dummyMsg.sampleIdentifier.first, dummyMsg.sampleIdentifier.second, dummyMsg.chunkIndex);
    }
    while(audioStreamer.flushToDiskTask.popMessage(TaskMessageTarget::AudioThread, dummyMsg)){
                    DEBUG_PRINTF("StreamingBuffer::mutate popping msg to empty with sampleId: %d %d and chunkIndex %d\n", dummyMsg.sampleIdentifier.first, dummyMsg.sampleIdentifier.second, dummyMsg.chunkIndex);

    }
    while(audioStreamer.streamSamplesTask.popMessage(TaskMessageTarget::TaskThread, dummyMsg)){
                    DEBUG_PRINTF("StreamingBuffer::mutate popping msg to empty with sampleId: %d %d and chunkIndex %d\n", dummyMsg.sampleIdentifier.first, dummyMsg.sampleIdentifier.second, dummyMsg.chunkIndex);

    }
    while(audioStreamer.flushToDiskTask.popMessage(TaskMessageTarget::TaskThread, dummyMsg)){
                    DEBUG_PRINTF("StreamingBuffer::mutate popping msg to empty with sampleId: %d %d and chunkIndex %d\n", dummyMsg.sampleIdentifier.first, dummyMsg.sampleIdentifier.second, dummyMsg.chunkIndex);

    }
    //wait till all ongoing work is done.
    task_sleep_ns(1e8);
}

void StreamingBuffer::mutate(StreamingMessage msg){
    assert(chunkStates != nullptr && "StreamingBuffer::mutate chunkStates null");
    if(msg.type == StreamingMessageType::releaseMutateOngoing){
        mutateOngoing.store(false, std::memory_order_acquire);
    }
    if(msg.type == StreamingMessageType::InitializeSample){
        assert(msg.chunkIndex > 0 && "StreamingBuffer::mutate InitializeSample with non-positive chunkIndex");
        auto insertResult = chunkIndicesInBuffer.insert(std::make_pair(msg.sampleIdentifier, std::vector<int>()));
        std::vector<int>& localChunkIndicesInBuffer = insertResult.first->second;
        /*
        DEBUG_PRINTF("StreamingBuffer::mutate InitializeSample %d_%d chunks=%d%s\n",
            msg.sampleIdentifier.first, msg.sampleIdentifier.second, msg.chunkIndex,
            insertResult.second ? " (new)" : " (reset)");
            */
            
        if(!insertResult.second){
            for(int chunkIndexInBuffer: localChunkIndicesInBuffer){
                if(chunkIndexInBuffer != CHUNK_INVALID){
                    assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < chunks.size() && "StreamingBuffer::mutate reset chunkIndexInBuffer out of range");
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
        for(size_t chunkStateIndex = 0; chunkStateIndex < static_cast<size_t>(totalNumberOfChunks); chunkStateIndex++){
            chunkStates[chunkStateIndex].set({-1,-1}, -1, false);
        }
    }
}


StreamingBufferIterator& StreamingBuffer::begin(SampleIdentifier sampleIdentifier, SBIType type){
    assert(chunkStates != nullptr && "StreamingBuffer::begin chunkStates null");
    assert(!chunks.empty() && "StreamingBuffer::begin chunks empty");
    assert(!iterators.empty() && "StreamingBuffer::begin iterators empty");
    assert(sampleIdentifier.first >= 0 && sampleIdentifier.first < 128 && "StreamingBuffer::begin key out of MIDI range");
    assert(sampleIdentifier.second >= 0 && sampleIdentifier.second < 128 && "StreamingBuffer::begin velocity out of MIDI range");
    auto lengthIt = availableSamples.find(sampleIdentifier);
    if(lengthIt == availableSamples.end()){
        throw std::runtime_error("StreamingBuffer::begin: sampleIdentifier not in availableSamples");
    }
    const size_t sampleLength = lengthIt->second;
    assert(sampleLength > 0 && "StreamingBuffer::begin sample length must be > 0");
    if(sampleLength == 0){
        throw std::runtime_error("StreamingBuffer::begin: sample length is zero");
    }
    const int expectedChunks = static_cast<int>(sampleLength / chunkLength) + 1;
    auto insertResult = chunkIndicesInBuffer.insert(std::make_pair(sampleIdentifier, std::vector<int>()));
    bool inserted = insertResult.second;
    std::vector<int>& localChunkIndicesInBuffer = insertResult.first->second;
    /*
    DEBUG_PRINTF("StreamingBuffer::begin sample %d_%d type=%d inserted=%d chunkindicesinbuffer.size() = %zu\n",
        sampleIdentifier.first, sampleIdentifier.second, static_cast<int>(type), inserted ? 1 : 0, localChunkIndicesInBuffer.size());
    */
        for(int i = 0; i < totalNumberOfIterators; i++){
        StreamingBufferIteratorAssignIndex = (StreamingBufferIteratorAssignIndex + 1) % totalNumberOfIterators;
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
    assert(expectedChunks == static_cast<int>(localChunkIndicesInBuffer.size()) && "StreamingBuffer::begin chunk count mismatch with sample length");
    if(expectedChunks != static_cast<int>(localChunkIndicesInBuffer.size())){
        throw std::runtime_error("StreamingBuffer::begin: chunkIndicesInBuffer size mismatch");
    }
    assert(0 < localChunkIndicesInBuffer.size() && "StreamingBuffer::begin chunk 0 missing");
    if (localChunkIndicesInBuffer.at(0) == CHUNK_INVALID) {
        int chunkIndexInBuffer = findFreeChunk();
        assert(0 < localChunkIndicesInBuffer.size() && "StreamingBuffer::begin chunk 0 missing before assign");
        localChunkIndicesInBuffer.at(0) = chunkIndexInBuffer;
        StreamingMessage outMsg{StreamingMessageType::AssignChunk, sampleIdentifier, 0, chunkIndexInBuffer};
        audioStreamer.streamSamplesTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
        if(type == SBIType::Read){
            throw std::runtime_error("start chunk should always be ready when a read iterator is made");           
        }
        if(type == SBIType::Write){  
            chunkStates[chunkIndexInBuffer].set(sampleIdentifier, 0, true);
        }
    }

    protectSample(sampleIdentifier);

    iterators.at(StreamingBufferIteratorAssignIndex).set(sampleIdentifier, 0, this, &localChunkIndicesInBuffer, type);
    assert(StreamingBufferIteratorAssignIndex < iterators.size() && "StreamingBuffer::begin return index out of range");
    /*
    rt_printf("::begin returned iterator %d", StreamingBufferIteratorAssignIndex);
    printIterators();
    rt_printf("\n\n\n");
    */
    return iterators.at(StreamingBufferIteratorAssignIndex);
}

void StreamingBuffer::sendStreamStartsMessages(){
    assert(chunkStates != nullptr && "StreamingBuffer::streamStarts chunkStates null");
    for(auto& sampleInfo: availableSamples){
        SampleIdentifier sampleIdentifier = sampleInfo.first;
        auto mapIt = chunkIndicesInBuffer.find(sampleIdentifier);
        if(mapIt == chunkIndicesInBuffer.end()){
            throw std::runtime_error(" StreamingBuffer::sendStreamStartsMessages: chunkIndicesInBuffer missing sample");
        }
        std::vector<int>& chunkIndicesInBuffer = mapIt->second;
        assert(!chunkIndicesInBuffer.empty() && "AudioStreamer::sendStreamStartsMessage empty chunkIndicesInBuffer");
        if(chunkIndicesInBuffer.at(0) == CHUNK_INVALID)
            audioStreamer.sendStreamChunkMessage(sampleIdentifier, 0);
    }
}

void StreamingBuffer::eraseIterator(StreamingBufferIterator& iterator){
    auto it = std::find_if(iterators.begin(), iterators.end(),
        [&iterator](const StreamingBufferIterator& elem) {
            return &elem == &iterator;
        }); //check if adress of elem is equal to address of iterator
    //DEBUG_RT_PRINTF("erasing iterator: %d\n", it - iterators.begin());
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
    auto mapIt = chunkIndicesInBuffer.find(sampleIdentifier);
    if(mapIt == chunkIndicesInBuffer.end()){
        throw std::runtime_error("StreamingBuffer::streamFullSample: chunkIndicesInBuffer missing sample");
    }
    std::vector<int>& chunkIndicesInBuffer = mapIt->second;
    assert(!chunkIndicesInBuffer.empty() && "StreamingBuffer::streamFullSample empty chunkIndicesInBuffer");
    int numberOfChunks = chunkIndicesInBuffer.size();
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "AudioStreamer::streamFromDisk chunkIndex out of range");
        if(chunkIndicesInBuffer.at(chunkIndex) == CHUNK_INVALID)
            audioStreamer.sendStreamChunkMessage(sampleIdentifier, chunkIndex);
    }
}

void StreamingBuffer::protectSample(SampleIdentifier sampleIdentifier){
    auto mapIt = chunkIndicesInBuffer.find(sampleIdentifier);
    if(mapIt == chunkIndicesInBuffer.end()){
        throw std::runtime_error("protectSample on uninitialized sampleIdentifier");
    }
    std::vector<int>& localChunkIndicesInBuffer = mapIt->second;
    assert(!localChunkIndicesInBuffer.empty() && "protectSample chunkIndicesInBuffer empty");
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
    auto mapIt = chunkIndicesInBuffer.find(sampleIdentifier);
    if(mapIt == chunkIndicesInBuffer.end()){
        throw std::runtime_error("unProtectSample on uninitialized sampleIdentifier");
    }
    std::vector<int>& localChunkIndicesInBuffer = mapIt->second;
    assert(!localChunkIndicesInBuffer.empty() && "unProtectSample chunkIndicesInBuffer empty");
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
    StreamingMessage outMsg{StreamingMessageType::InitializeSample, sampleIdentifier, numberOfChunks, -1};
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
}

void StreamingBuffer::clear(){
    //lock other threads from working
    mutateOngoing.store(true, std::memory_order_acquire);
    //first pop all remaining messages and wait for the ongoing taskWorkMessages to finish.
    this->stopWork();
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, {StreamingMessageType::Clear, {-1,-1}, -1, -1});
    mutateDataTask.taskCheckAndWorkMessages();
}

void StreamingBuffer::initializeForNewSamplePack(std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    //should be run by an external non-Audio thread
    sendInitializeSamplesMessages(availableSamples);
    //mutateDataTask.taskCheckAndWorkMessages();
    mutateDataTask.pushMessage(TaskMessageTarget::TaskThread, {StreamingMessageType::releaseMutateOngoing, {-1,-1}, -1, -1});
    mutateDataTask.taskCheckAndWorkMessages();
    task_sleep_ns(1e8);
    sendStreamStartsMessages();
    audioStreamer.streamSamplesTask.taskCheckAndWorkMessages();
    //now loads the starts and sends back info about the chunks to the audio thread
    task_sleep_ns(1e8);
    audioCheckAndWorkMessages();
}

void StreamingBuffer::printInfo(){
    rt_printf("StreamingBuffer info:\n");
    rt_printf("  name=%s path=%s\n", bufferName.c_str(), folderPath.c_str());
    rt_printf("  chunkLength=%d totalChunks=%d iterators=%d\n", chunkLength, totalNumberOfChunks, totalNumberOfIterators);
    rt_printf("  chunks size=%zu chunkStates=%s\n", chunks.size(), chunkStates ? "yes" : "no");
    rt_printf("  availableSamples=%zu chunkIndicesInBuffer=%zu\n", availableSamples.size(), chunkIndicesInBuffer.size());
    int counter = 0;
    for(auto& availableSample: availableSamples){
        counter++;
        if(counter > 4) break;
        SampleIdentifier sampleIdentifier = availableSample.first;
        rt_printf("chunkIndicesInBuffer[%d,%d] = {%d, %d, ...\n", sampleIdentifier.first, sampleIdentifier.second, chunkIndicesInBuffer[sampleIdentifier].at(0), chunkIndicesInBuffer[sampleIdentifier].at(1));

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
    return -1;
}
