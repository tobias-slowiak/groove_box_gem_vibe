#include "../../include/audio/Recorder.h"

static constexpr size_t RECORDING_BUFFER_SIZE = 44100 * 10;
static constexpr int recorderFlushTaskPrio = 50;

Recorder::Recorder(ResourceManager& resourceManager, std::string filename)
    : resourceManager(resourceManager),
    filename(filename),
    recordingBuffers({std::vector<float>(RECORDING_BUFFER_SIZE, 0.f),
                      std::vector<float>(RECORDING_BUFFER_SIZE, 0.f)}),
    flushTask(this, recorderFlushTaskPrio, flushTaskName),
    wavWriter(filename, resourceManager)
{

}

void Recorder::setNewFilename(std::string newFilename)
{
    filename = newFilename;
    wavWriter = WavWriter(filename, resourceManager);
}

void Recorder::startRecording()
{
    rt_printf("Recorder: starting recording\n");
    isRecordingFlag = true;
}
void Recorder::stopRecording()
{
    rt_printf("Recorder: stopping recording\n");
    isRecordingFlag = false;
    flushTask.pushMessage(TaskMessageTarget::TaskThread, RecorderMessage{currentBufferIndex, positionInSample, true});
    positionInSample = 0;
    currentBufferIndex = 0;
}
bool Recorder::isRecording()
{
    return isRecordingFlag;
}

void Recorder::taskWorkMessage(std::string& taskName, RecorderMessage msg)
{
    if(taskName == flushTaskName){
        if(msg.finishRecording){
            rt_printf("Recorder: finishing recording\n");
            wavWriter.writeChunk(f_numberOfBuffersFlushed * RECORDING_BUFFER_SIZE,
                             VEC_AT(recordingBuffers, msg.bufferToFlush),
                             msg.remainingBufferSize);
            wavWriter.truncateFile(f_numberOfBuffersFlushed * RECORDING_BUFFER_SIZE + msg.remainingBufferSize);
            f_numberOfBuffersFlushed = 0;
            return;
        }
        rt_printf("Recorder: flush task received message\n");
        wavWriter.writeChunk(f_numberOfBuffersFlushed * RECORDING_BUFFER_SIZE,
                             VEC_AT(recordingBuffers, msg.bufferToFlush),
                             RECORDING_BUFFER_SIZE);
        f_numberOfBuffersFlushed++;
    }
}

void Recorder::processBlockwise()
{
    flushTask.taskCheckAndWorkMessages();
}

void Recorder::process(float inFrame)
{
    if(currentBufferIndex < 0 || currentBufferIndex > 1){
        throw std::runtime_error("Recorder: currentBufferIndex out of range");
    }
    std::vector<float>& currentBuffer = recordingBuffers[currentBufferIndex];
    if(isRecordingFlag){
        VEC_AT(currentBuffer, positionInSample) = inFrame;
        positionInSample++;
        if(positionInSample >= RECORDING_BUFFER_SIZE){
            flushTask.pushMessage(TaskMessageTarget::TaskThread, RecorderMessage{currentBufferIndex});
            currentBufferIndex = (currentBufferIndex + 1) % recordingBuffers.size();
            positionInSample = 0;
            rt_printf("Recorder: switched to sample %d\n", currentBufferIndex);
        }
    }
}
