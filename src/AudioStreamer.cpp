#include "../include/AudioStreamer.h"
#include "../include/StreamingBuffer.h"
#include <algorithm>
#include <stdexcept>
//compile
AudioStreamer::AudioStreamer(StreamingBuffer& streamingBuffer)
        : parent(streamingBuffer),
        streamTaskName(parent.bufferName + "_STask"),
        flushTaskName(parent.bufferName + "_FTask"),
        streamTask(this, streamTaskPrio, streamTaskName),
        flushTask(this, flushTaskPrio, flushTaskName),
        s_streamBuffer(std::vector<float>(parent.chunkLength)),
        f_flushBuffer(std::vector<float>(parent.chunkLength)){}



void AudioStreamer::sendStreamChunkMessage(SampleIdentifier sampleIdentifier, int chunkIndex, std::vector<int>& chunkIndicesInBuffer){
    assert(chunkIndex < chunkIndicesInBuffer.size());
    int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
    if(chunkIndexInBuffer == CHUNK_INVALID)
        chunkIndexInBuffer = parent.assignToFreeChunk(sampleIdentifier, chunkIndex, chunkIndicesInBuffer);
    StreamingMessage outMsg{StreamingMessageType::StreamChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer};
    streamTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
}

void AudioStreamer::sendFlushChunksMessages(SampleIdentifier sampleIdentifier){ // audiothread sending messages to flush thread
    //A possible improvement would be to only protect chunks that are not flushed yet to have more space
    //to write to but that is not necessary at the moment because space is not an issue
    pendingFlushes.insert(sampleIdentifier);
    parent.protectSample(sampleIdentifier);
    std::vector<int>& chunkIndicesInBuffer = parent.getChunkIndicesInBuffer(sampleIdentifier);
    assert(!chunkIndicesInBuffer.empty() && "AudioStreamer::flushToDisk empty chunkIndicesInBuffer");
    int numberOfChunks = chunkIndicesInBuffer.size();
    
    StreamingMessage outMsg{StreamingMessageType::FlushInfo, sampleIdentifier, numberOfChunks, numberOfChunks};
    flushTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
    for(int chunkIndex = 0; chunkIndex < numberOfChunks; chunkIndex++){
        assert(chunkIndex >= 0 && static_cast<size_t>(chunkIndex) < chunkIndicesInBuffer.size() && "AudioStreamer::flushToDisk chunkIndex out of range");
        int chunkIndexInBuffer = chunkIndicesInBuffer.at(chunkIndex);
        assert(chunkIndexInBuffer != CHUNK_INVALID && "AudioStreamer::flushToDisk trying to flush invalid chunk, should not happen");
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::flushToDisk chunkIndexInBuffer out of range");
        StreamingMessage outMsg{StreamingMessageType::FlushChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer};
        flushTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
    }
}

void AudioStreamer::workStreamMessage(StreamingMessage msg){ //stream thread only
    if(parent.mutateOngoing.load(std::memory_order_acquire)){
        if(!streamTask.tryReleaseInFlight()){
            DEBUG_PRINTF("Error: try to realease in flighht due to mutate and not successfull\n");
        }
        return;
    }
    int chunkIndexInBuffer = msg.chunkIndexInBuffer;
    int chunkIndex = msg.chunkIndex;
    if(msg.type == StreamingMessageType::StreamChunk){
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
            sdFileReadEndIndex = sampleLengthInFrames - 1;
            //TODO: check if audiofileutiilities::getsamples is not overwriting the end of sample sentinels.
            for(size_t i = sdFileReadEndIndex - sdFileReadStartIndex; i < parent.chunkLength; i++){
                assert(s_streamBuffer.size() > i && "AudioStreamer::stream s_streamBuffer sentinel write out of range");
                s_streamBuffer.at(i) = END_OF_SAMPLE;
            }
        }
        if(AudioFileUtilities::getSamples(parent.filename(msg.sampleIdentifier), s_streamBuffer.data(), channel,
                                            sdFileReadStartIndex, sdFileReadEndIndex) == 0) {
            assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::stream chunkIndexInBuffer out of range during copy");
            for(size_t i = 0; i < parent.chunkLength; i++){
                assert(parent.chunks.at(chunkIndexInBuffer).size() > i && "AudioStreamer::stream parent chunk write out of range");
                assert(s_streamBuffer.size() > i && "AudioStreamer::stream s_streamBuffer out of range");
                parent.chunks.at(chunkIndexInBuffer).at(i) = s_streamBuffer.at(i);
            }
            parent.chunkStates[chunkIndexInBuffer].chunkReady.store(true, std::memory_order_release);
        } else {
            throw std::runtime_error("StreamingBuffer::initForFolder: failed to load sample"
            + parent.filename(msg.sampleIdentifier) + " sdfilereadendindex = " + std::to_string(sdFileReadEndIndex) +
            + "sampleLengthInFrames = " + std::to_string(sampleLengthInFrames));
        }
    }
}


void AudioStreamer::workFlushMessage(StreamingMessage msg){ //flush thread only
    if(parent.mutateOngoing.load(std::memory_order_acquire)){
        if(!streamTask.tryReleaseInFlight()){
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
                if(!streamTask.tryReleaseInFlight()){
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
            StreamingMessage outMsg{StreamingMessageType::FlushComplete, msg.sampleIdentifier, ARBITRARY_VALUE, ARBITRARY_VALUE};
            flushTask.pushMessage(TaskMessageTarget::AudioThread, outMsg);
        }
    }
}

void AudioStreamer::taskWorkMessage(std::string& taskName, StreamingMessage msg){
	if(taskName == parent.bufferName + "_STask"){
		workStreamMessage(msg);
		return;
	}
    if(taskName == parent.bufferName + "_FTask"){
		workFlushMessage(msg);
		return;
	}
	throw std::runtime_error("AudioStreamer::taskWorkMessage invoked with task name " + taskName);
}

void AudioStreamer::audioCheckAndWorkMessages(){
    StreamingMessage msg;
    while(flushTask.popMessage(TaskMessageTarget::AudioThread, msg)){
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
    this->audioCheckAndWorkMessages();
    if(!parent.mutateOngoing.load(std::memory_order_acquire)){
        streamTask.taskCheckAndWorkMessages();
        flushTask.taskCheckAndWorkMessages();
    }
}
