#pragma once
#include <Bela.h>
#include <string>
#include <atomic>
#include <stdexcept>
#include <vector>
#include <cassert>
//compiel
#include "../hardwareInterfaces/DisplayMessage.h"

/*each daughter class must implement
  taskWorkMessage(std::string& taskName, MsgType msg)
    which tells the taskwrapper what to do with a message

other than that it should have fields with name
  X_...
if they are a second instance of fields of the given class
which can only be used by the task and must be synchronized.
X is the first letter of the stream name.

*/

/*
WORKFLOW
Communication between threads via messages.
each class that owns a taskwrapper needs to be extern ... defined here.
each class that owns a taskwrapper needs to do
processBlockwise() which does taskCheckAndWorkMessages();
and also audioThreadCheckandworkmessages() if it needs to do things on the audio thread.
*/

//taskmessage for tasks that only do one thing.
struct DefaultTaskMessage{};

enum class TaskMessageTarget: int{
    AudioThread,
    TaskThread
};

static constexpr size_t DEFAULT_TASK_MESSAGE_QUEUE_CAPACITY = 1024;

template<typename MsgType>
class TaskMessageQueue {
public:
    TaskMessageQueue() = delete;
    
    TaskMessageQueue(size_t capacity, std::string name);
    
    bool push(MsgType msg);

    bool pop(MsgType& msg);

private:
    size_t capacity;
    std::string name;
    std::vector<MsgType> messages;
    std::atomic<size_t> tail{0};
    std::atomic<size_t> head{0}; // producer-only
};

template<typename ParentType, typename MsgType>
class TaskWrapper{
public:
    TaskWrapper(ParentType* object, int priority, std::string name);

    void setScheduleFlag();

    bool isInFlight();

    bool tryClaimInFlight();

    void taskCheckAndWorkMessages();

    bool tryReleaseInFlight();

    std::string& getName();

    bool pushMessage(TaskMessageTarget target, MsgType msg);

    bool popMessage(TaskMessageTarget target, MsgType& msg);

private:
    ParentType* object;
    AuxiliaryTask auxiliaryTask;
    std::atomic<bool> taskInFlight{false};
    int priority;
    std::string name;
    bool needsScheduling = false;

    static void taskWorkMessages(void* arg);

    TaskMessageQueue<MsgType> audioToTask{DEFAULT_TASK_MESSAGE_QUEUE_CAPACITY, name+"_audioToTask"};
    TaskMessageQueue<MsgType> taskToAudio{DEFAULT_TASK_MESSAGE_QUEUE_CAPACITY, name+"_taskToAudio"};
};

// Forward declarations for explicit instantiations.
class DisplayContextReal;
class AudioStreamer;
class StreamingBuffer;
struct StreamingMessage;
class SamplePack;
class Recorder;
struct RecorderMessage;

//so the way i understand the follwing is that if i do not do this,
//the task wrapper class for a certain template type is compiled by the 
//corresponding class so when i make a task wrapper in the streamingbuffer, then
//it compiles it's own taskwrapper, and i had problems with this because when i
//change taskwrapper.cpp it is recompiled, but the taskwrapper version of
//streamingbuffer is not recompiled and so the changes are not seen in the program
//and while i am still developing taskwrapper that is bad, i could change it later
//when it is fully fixed.
extern template class TaskWrapper<DisplayContextReal, DisplayMessage>;
extern template class TaskWrapper<AudioStreamer, StreamingMessage>;
extern template class TaskWrapper<StreamingBuffer, StreamingMessage>;
extern template class TaskWrapper<SamplePack, DefaultTaskMessage>;
extern template class TaskWrapper<Recorder, RecorderMessage>;