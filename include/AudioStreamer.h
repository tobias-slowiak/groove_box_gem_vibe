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

    void sendStreamChunkMessage(SampleIdentifier sampleIdentifier, int chunkIndex);

    void flushToDisk(SampleIdentifier sampleIdentifier);

    void audioCheckAndWorkMessages();
    
    void processBlockwise();

    bool streamerIsInFlight(){return streamSamplesTask.isInFlight();}

    //Stream Thread only

    void stream(StreamingMessage msg);

    //Flush Thread only
    void flush(StreamingMessage msg);

    void taskWorkMessage(std::string& taskName, StreamingMessage msg);

private:
    enum class ScheduleStatus {
        Scheduled,
        Busy,
        Error
    };
    friend class StreamingBuffer;
    friend class StreamingBufferIterator;
    friend class SamplePack;
    StreamingBuffer& parent;
    

    //Auxiliary Tasks
    int streamSamplePrio = 50;
    int flushToDiskPrio = 20;
    std::string streamSamplesTaskName;
    std::string flushToDiskTaskName;
    TaskWrapper<AudioStreamer, StreamingMessage> streamSamplesTask;
    TaskWrapper<AudioStreamer, StreamingMessage> flushToDiskTask;

    //Stream Thread Only
    std::unordered_map<SampleIdentifier, std::vector<int>> s_chunkIndicesInBuffer; //synchronize with parent
    std::vector<float> s_streamBuffer;

    std::unordered_set<SampleIdentifier> pendingFlushes;
    //Flush Thread Only
    std::unordered_map<SampleIdentifier, int> f_numberOfFlushableChunks;
    std::vector<float> f_flushBuffer;
    std::unordered_map<SampleIdentifier, WavWriter> f_wavWriters;

};
