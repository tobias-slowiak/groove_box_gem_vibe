#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <Bela.h>
#include "../streamingBuffer/StreamingBuffer.h"
#include "../general/TaskWrapper.h"
#include "../streamingBuffer/WavWriter.h"

class ResourceManager;

struct RecorderMessage{
    RecorderMessage() = default;
    RecorderMessage(int bufferToFlush): bufferToFlush(bufferToFlush){}
    RecorderMessage(int bufferToFlush, int remainingBufferSize, bool finishRecording)
        : bufferToFlush(bufferToFlush), remainingBufferSize(remainingBufferSize), finishRecording(finishRecording){}
    int bufferToFlush;
    int remainingBufferSize = 0;
    bool finishRecording = false;
};

class Recorder {
public:
    Recorder(ResourceManager& resourceManager, std::string filename);

    void setNewFilename(std::string newFilename);
    void startRecording();
    void stopRecording();
    bool isRecording();

    void taskWorkMessage(std::string& taskName, RecorderMessage msg);

    void processBlockwise();
    void process(float inFrame);

private:
    ResourceManager& resourceManager;
    std::string filename;

    std::vector<std::vector<float>> recordingBuffers;

    std::string flushTaskName = "Recorder_FTask";
    TaskWrapper<Recorder, RecorderMessage> flushTask;
    int f_numberOfBuffersFlushed = 0;
    WavWriter wavWriter;

    bool isRecordingFlag = false;
    int currentBufferIndex = 0;
    int positionInSample = 0;
};
