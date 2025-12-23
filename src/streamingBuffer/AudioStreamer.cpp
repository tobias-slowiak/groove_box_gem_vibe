#include "../../include/streamingBuffer/AudioStreamer.h"
#include "../../include/streamingBuffer/StreamingBuffer.h"
#include <algorithm>
#include <stdexcept>
#include "../../include/general/BasicUtilities.h"
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
    int chunkIndexInBuffer = VEC_AT(chunkIndicesInBuffer, chunkIndex);
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
        int chunkIndexInBuffer = VEC_AT(chunkIndicesInBuffer, chunkIndex);
        assert(chunkIndexInBuffer != CHUNK_INVALID && "AudioStreamer::flushToDisk trying to flush invalid chunk, should not happen");
        assert(chunkIndexInBuffer >= 0 && static_cast<size_t>(chunkIndexInBuffer) < parent.chunks.size() && "AudioStreamer::flushToDisk chunkIndexInBuffer out of range");
        StreamingMessage outMsg{StreamingMessageType::FlushChunk, sampleIdentifier, chunkIndex, chunkIndexInBuffer};
        flushTask.pushMessage(TaskMessageTarget::TaskThread, outMsg);
    }
}

void AudioStreamer::workStreamMessage(StreamingMessage msg){ //stream thread only
    if(parent.mutateOngoing.load(std::memory_order_acquire)){
        if(!streamTask.tryReleaseInFlight()) DEBUG_PRINTF("Warning: try to realease in flighht due to mutate and not successfull\n");
        return;
    }
    if(msg.type == StreamingMessageType::StreamChunk){
        int chunkIndexInBuffer = msg.chunkIndexInBuffer;
        int chunkIndex = msg.chunkIndex;
        int channel = 0; //maybe make stereo possible at some point
        size_t sdFileReadStartIndex = chunkIndex * parent.chunkLength;
        size_t sdFileReadEndIndex = sdFileReadStartIndex + parent.chunkLength;
        size_t sampleLengthInFrames = parent.getSampleLength(msg.sampleIdentifier);
        if(sdFileReadStartIndex == sampleLengthInFrames){return;} //sample is exact multiple of chunkLength and everything has already been loaded 
        if(sdFileReadEndIndex > sampleLengthInFrames){
            sdFileReadEndIndex = sampleLengthInFrames;
            for(size_t i = sdFileReadEndIndex - sdFileReadStartIndex; i < parent.chunkLength; i++){
                VEC_AT(s_streamBuffer, i) = END_OF_SAMPLE;
            }
        }
        if(AudioFileUtilities::getSamples(parent.filename(msg.sampleIdentifier), s_streamBuffer.data(), channel, sdFileReadStartIndex, sdFileReadEndIndex) == 0) {
            std::vector<float>& chunk = VEC_AT(parent.chunks, chunkIndexInBuffer);
            for(size_t i = 0; i < parent.chunkLength; i++){
                VEC_AT(chunk, i) = VEC_AT(s_streamBuffer, i);
            }
            VEC_AT(parent.chunkStates, chunkIndexInBuffer).chunkReady.store(true, std::memory_order_release);
        } else {
            throw std::runtime_error("StreamingBuffer::initForFolder: failed to load sample" + parent.filename(msg.sampleIdentifier) + " sdfilereadstartindex = " + std::to_string(sdFileReadStartIndex) + " sdfilereadendindex = " + std::to_string(sdFileReadEndIndex) + "sampleLengthInFrames = " + std::to_string(sampleLengthInFrames));
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
        auto& chunk = VEC_AT(parent.chunks, msg.chunkIndexInBuffer);
        for(size_t i = 0; i < parent.chunkLength; i++){
            float nextFrame = VEC_AT(chunk, i);
            if(nextFrame == END_OF_SAMPLE){
                if(!streamTask.tryReleaseInFlight()){
                    DEBUG_PRINTF("Error: try to realease in flighht due to mutate and not successfull\n");
                }
                return;
            }
            VEC_AT(f_flushBuffer, i) = nextFrame;
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

            int sampleInUseRes = parent.sampleInUse(msg.sampleIdentifier);
            if(sampleInUseRes == FLUSH_EXISTS || sampleInUseRes == BOTH_EXIST) throw std::runtime_error("other flush exists even though i just erased.");
            if(!sampleInUseRes) parent.unProtectSample(msg.sampleIdentifier);
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
