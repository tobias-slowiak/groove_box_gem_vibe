#include "../include/AudioStreamer.h"
#include "../include/StreamingBuffer.h"
#include <algorithm>
#include <stdexcept>
//compile
AudioStreamer::AudioStreamer(StreamingBuffer& streamingBuffer)
        : parent(streamingBuffer),
        streamSamplesTaskName(parent.bufferName + "_STask"),
        flushToDiskTaskName(parent.bufferName + "_FTask"),
        streamSamplesTask(this, streamSamplePrio, streamSamplesTaskName),
        flushToDiskTask(this, flushToDiskPrio, flushToDiskTaskName),
        s_streamBuffer(std::vector<float>(parent.chunkLength)),
        f_flushBuffer(std::vector<float>(parent.chunkLength)){}

void AudioStreamer::sendStreamChunkMessage(SampleIdentifier sampleIdentifier, int chunkIndex){
    StreamingMessage outMsg{StreamingMessageType::StreamChunk, sampleIdentifier, chunkIndex, -1};
    streamSamplesTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
}


void AudioStreamer::taskWorkMessage(std::string& taskName, StreamingMessage msg){
	if(taskName == parent.bufferName + "_STask"){
		stream(msg);
		return;
	}
    if(taskName == parent.bufferName + "_FTask"){
		flush(msg);
		return;
	}
	throw std::runtime_error("AudioStreamer::taskWorkMessage invoked with task name " + taskName);
}


void AudioStreamer::stream(StreamingMessage msg){ //stream thread only
    if(parent.mutateOngoing.load(std::memory_order_acquire)){
        if(!streamSamplesTask.tryReleaseInFlight()){
            DEBUG_PRINTF("Error: try to realease in flighht due to mutate and not successfull\n");
        }
        return;
    }
    auto mapIt = s_chunkIndicesInBuffer.find(msg.sampleIdentifier);
    if(mapIt == s_chunkIndicesInBuffer.end()){
        throw std::runtime_error("AudioStreamer::stream: missing s_chunkIndicesInBuffer entry for sample");
    }
    std::vector<int>& chunkIndicesInBuffer = mapIt->second;
    assert(!chunkIndicesInBuffer.empty() && "AudioStreamer::stream empty chunkIndicesInBuffer");
    int chunkIndexInBuffer = msg.chunkIndexInBuffer;
    int chunkIndex = msg.chunkIndex;
    if(msg.type == StreamingMessageType::StreamChunk){
        if(chunkIndexInBuffer == -1 || chunkIndexInBuffer == CHUNK_INVALID){
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
            DEBUG_PRINTF("AudioStreamer::stream: avaialble samples seen:\n");
            for(auto& pairthing: parent.availableSamples){
                DEBUG_PRINTF("%d_%d: %zu\n", pairthing.first.first, pairthing.first.second, pairthing.second);
            }
            throw std::runtime_error("AudioStreamer::stream: sample to stream not in available samples: " + std::to_string(msg.sampleIdentifier.first) + "_" + std::to_string(msg.sampleIdentifier.second) + " for chunk Index: " + std::to_string(msg.chunkIndex));
        }
        size_t sampleLengthInFrames = lengthIt->second;
        if(sdFileReadEndIndex > sampleLengthInFrames){
            sdFileReadEndIndex = sampleLengthInFrames;
            //TODO: check if audiofileutiilities::getsamples is not overwriting the end of sample sentinels.
            for(size_t i = sdFileReadEndIndex - sdFileReadStartIndex; i < parent.chunkLength; i++){
                assert(s_streamBuffer.size() > i && "AudioStreamer::stream s_streamBuffer sentinel write out of range");
                s_streamBuffer.at(i) = END_OF_SAMPLE;
            }
        }
        if(AudioFileUtilities::getSamples(parent.filename(msg.sampleIdentifier),
                    s_streamBuffer.data(), channel,
                    sdFileReadStartIndex, sdFileReadEndIndex) == 0) {
            assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::stream chunkIndexInBuffer out of range during copy");
            for(size_t i = 0; i < parent.chunkLength; i++){
                assert(parent.chunks.at(chunkIndexInBuffer).size() > i && "AudioStreamer::stream parent chunk write out of range");
                assert(s_streamBuffer.size() > i && "AudioStreamer::stream s_streamBuffer out of range");
                parent.chunks.at(chunkIndexInBuffer).at(i) = s_streamBuffer.at(i);
            }
            parent.chunkStates[chunkIndexInBuffer].set(msg.sampleIdentifier, chunkIndex, true);
            StreamingMessage outMsg{StreamingMessageType::ChunkReady, msg.sampleIdentifier, chunkIndex, chunkIndexInBuffer};
            streamSamplesTask.pushMessage(TaskMessageTarget::AudioThread, outMsg);
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
            throw std::runtime_error("StreamingBuffer::initForFolder: failed to load sample"
            + parent.filename(msg.sampleIdentifier));
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
}

void AudioStreamer::flushToDisk(SampleIdentifier sampleIdentifier){ // audiothread sending messages to flush thread
    //A possible improvement would be to only protect chunks that are not flushed yet to have more space
    //to write to but that is not necessary at the moment because space is not an issue
    pendingFlushes.insert(sampleIdentifier);
    parent.protectSample(sampleIdentifier);
    auto mapIt = parent.chunkIndicesInBuffer.find(sampleIdentifier);
    if(mapIt == parent.chunkIndicesInBuffer.end()){
        throw std::runtime_error("AudioStreamer::flushToDisk: chunkIndicesInBuffer missing sample");
    }
    std::vector<int>& chunkIndicesInBuffer = mapIt->second;
    assert(!chunkIndicesInBuffer.empty() && "AudioStreamer::flushToDisk empty chunkIndicesInBuffer");
    int numberOfChunks = chunkIndicesInBuffer.size();
    
    StreamingMessage outMsg{StreamingMessageType::FlushInfo, sampleIdentifier, numberOfChunks, numberOfChunks};
    flushToDiskTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "AudioStreamer::flushToDisk chunkIndex out of range");
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer != CHUNK_INVALID && "AudioStreamer::flushToDisk trying to flush invalid chunk, should not happen");
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::flushToDisk chunkIndexInBuffer out of range");
        StreamingMessage outMsg{StreamingMessageType::FlushChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer};
        flushToDiskTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
    }
}


void AudioStreamer::flush(StreamingMessage msg){ //flush thread only
    if(parent.mutateOngoing.load(std::memory_order_acquire)){
        if(!streamSamplesTask.tryReleaseInFlight()){
            DEBUG_PRINTF("Error: try to realease in flighht due to mutate and not successfull\n");
        }
        return;
    }
    if(msg.type == StreamingMessageType::FlushInfo){
        f_wavWriters.insert(std::make_pair(
            msg.sampleIdentifier,
            WavWriter(parent.filename(msg.sampleIdentifier), parent.resourceManager, msg.chunkIndex * parent.chunkLength)
        ));
        f_numberOfFlushableChunks[msg.sampleIdentifier] = msg.chunkIndex;
    }
    if(msg.type == StreamingMessageType::FlushChunk){
        //potential concurrent read, however not so bad since 1 the chunk is still prtected to will not be written to and 2 with only 1 core
        //on the cpu i think concurrent reads on a constant array should not make too many problems.
        auto writerIt = f_wavWriters.find(msg.sampleIdentifier);
        if(writerIt == f_wavWriters.end()){
            throw std::runtime_error("AudioStreamer::flush: no wav writer for sampleIdentifier");
        }
        for(size_t i = 0; i < parent.chunkLength; i++){
            assert(msg.chunkIndexInBuffer >= 0 && static_cast<size_t>(msg.chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::flush chunkIndexInBuffer out of range");
            assert(parent.chunks.at(msg.chunkIndexInBuffer).size() > i && "AudioStreamer::flush chunk data out of range");
            float nextFrame = parent.chunks.at(msg.chunkIndexInBuffer).at(i);
            if(nextFrame == END_OF_SAMPLE){
                if(!streamSamplesTask.tryReleaseInFlight()){
                    DEBUG_PRINTF("Error: try to realease in flighht due to mutate and not successfull\n");
                }
                return;
            }
            assert(f_flushBuffer.size() > i && "AudioStreamer::flush f_flushBuffer out of range");
            f_flushBuffer.at(i) = nextFrame;
        }
        writerIt->second.writeChunk(msg.chunkIndex * parent.chunkLength, f_flushBuffer);
        if(msg.chunkIndex == f_numberOfFlushableChunks[msg.sampleIdentifier] - 1){
            f_wavWriters.erase(writerIt);
            StreamingMessage outMsg{StreamingMessageType::FlushComplete, msg.sampleIdentifier, -1, -1};
            flushToDiskTask.pushMessage(TaskMessageTarget::AudioThread, outMsg);
        }
    }
}

void AudioStreamer::audioCheckAndWorkMessages(){
    StreamingMessage msg;
    while(streamSamplesTask.popMessage(TaskMessageTarget::AudioThread, msg)){
        auto mapIt = parent.chunkIndicesInBuffer.find(msg.sampleIdentifier);
        if(mapIt == parent.chunkIndicesInBuffer.end()){
            throw std::runtime_error("AudioStreamer::processBlockwise: chunkIndicesInBuffer missing sample");
        }
        std::vector<int>& chunkIndicesInBuffer = mapIt->second;
        assert(!chunkIndicesInBuffer.empty() && "AudioStreamer::processBlockwise empty chunkIndicesInBuffer");
        if(msg.type == StreamingMessageType::ChunkReady){
            if(msg.chunkIndex == chunkIndicesInBuffer.size() - 1){
                DEBUG_RT_PRINTF("stream done for sample %d %d\n", msg.sampleIdentifier.first, msg.sampleIdentifier.second);
            }
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
    while(flushToDiskTask.popMessage(TaskMessageTarget::AudioThread, msg)){
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
}

void AudioStreamer::processBlockwise(){
    /////////////////////Message passing part.
    this->audioCheckAndWorkMessages();
    ///////////////////////task management part
    if(!parent.mutateOngoing.load(std::memory_order_acquire)){
        streamSamplesTask.taskCheckAndWorkMessages();
        flushToDiskTask.taskCheckAndWorkMessages();
    }
}
