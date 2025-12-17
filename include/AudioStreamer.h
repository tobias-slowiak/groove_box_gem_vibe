#pragma once
//compile
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/SampleIdentifier.h"
#include "../include/StreamingMessage.h"
#include "../include/TaskWrapper.h"
#include "../include/WavWriter.h"

class StreamingBuffer;


class AudioStreamer{
public:
    //Audio thread only 
    AudioStreamer(StreamingBuffer& streamingBuffer);

    void sendStreamChunkMessage(SampleIdentifier sampleIdentifier, int chunkIndex, std::vector<int>& chunkIndicesInBuffer);

    void sendFlushChunksMessages(SampleIdentifier sampleIdentifier);

    //Stream Thread only
    void workStreamMessage(StreamingMessage msg);

    //Flush Thread only
    void workFlushMessage(StreamingMessage msg);

    void taskWorkMessage(std::string& taskName, StreamingMessage msg);

    void audioCheckAndWorkMessages();
    
    void processBlockwise();

    bool streamerIsInFlight(){return streamTask.isInFlight();}

private:
    friend class StreamingBuffer;
    friend class StreamingBufferIterator;
    StreamingBuffer& parent;
    
    //Auxiliary Tasks
    int streamTaskPrio = 70;
    int flushTaskPrio = 20;
    std::string streamTaskName;
    std::string flushTaskName;
    TaskWrapper<AudioStreamer, StreamingMessage> streamTask;
    TaskWrapper<AudioStreamer, StreamingMessage> flushTask;

    //Stream Thread Only
    std::vector<float> s_streamBuffer;

    std::unordered_set<SampleIdentifier> pendingFlushes;
    //Flush Thread Only
    std::unordered_map<SampleIdentifier, int> f_numberOfFlushableChunks;
    std::vector<float> f_flushBuffer;
    std::unordered_map<SampleIdentifier, WavWriter> f_wavWriters;

};
