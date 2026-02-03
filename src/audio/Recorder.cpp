#include "../../include/audio/Recorder.h"

static constexpr size_t RECORDING_BUFFER_SIZE = 44100 * 10;

Recorder::Recorder(ResourceManager& resourceManager, std::string filename)
    : resourceManager(resourceManager),
    filename(filename),
    availableSamples({{{0,0}, RECORDING_BUFFER_SIZE}, {{0,1}, RECORDING_BUFFER_SIZE}}),
    streamingBuffer(resourceManager,
                RECORDING_BUFFER_SIZE * 2,
                2,
                "Recorder_Buffer_" + filename, "/mnt/sdcard/Samples/Recordings",
                availableSamples)
{
    streamingBuffer.initializeForLoopers(availableSamples);
    iterators.push_back(&streamingBuffer.begin({0,0}, SBIType::Write));
    iterators.push_back(&streamingBuffer.begin({0,1}, SBIType::Write));
}

void Recorder::startRecording()
{
    isRecordingFlag = true;
}
void Recorder::stopRecording()
{
    isRecordingFlag = false;
}

void Recorder::process(float inFrame)
{
    StreamingBufferIterator& iterator = *iterators[currentSample];
    if(isRecordingFlag){
        *iterator = inFrame;
        iterator++;
        positionInSample++;
        if(positionInSample >= RECORDING_BUFFER_SIZE){
            iterator.flush();
            iterator.rewind();
            currentSample = (currentSample + 1) % iterators.size();
            positionInSample = 0;
            rt_printf("Recorder: switched to sample %d\n", currentSample);
        }
    }
}
