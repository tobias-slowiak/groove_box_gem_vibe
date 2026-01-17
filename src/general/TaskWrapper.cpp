#include "../../include/general/TaskWrapper.h"
#include "../../include/streamingBuffer/AudioStreamer.h"
#include "../../include/general/DebugLog.h"
#include "../../include/hardwareInterfaces/DisplayContextReal.h"
#include "../../include/streamingBuffer/StreamingBuffer.h"
#include "../../include/streamingBuffer/StreamingMessage.h"
#include "../../include/audio/SamplePack.h"
//ATTENTION WHEN COMPILING! MIGHT DEMAND A -Rebuild!!

#include <errno.h>
#include <utility>

template<typename MsgType>
TaskMessageQueue<MsgType>::TaskMessageQueue(size_t capacity, std::string name) : capacity(capacity), name(name){
    assert(capacity > 2 && "TaskMessageQueue capacity must be > 2");
    messages.resize(capacity);
}

template<typename MsgType>
bool TaskMessageQueue<MsgType>::push(MsgType msg){
    size_t currentHead = head.load(std::memory_order_relaxed);
    size_t nextHead = (currentHead + 1) % capacity;
    size_t currentTail = tail.load(std::memory_order_acquire);
    if(nextHead == currentTail){
        throw std::runtime_error("TaskMessageQueue::push queue full, investigate! name="+name+"with capacity="+std::to_string(capacity)
        +" head="+std::to_string(currentHead)+" nextHead="+std::to_string(nextHead)+" tail="+std::to_string(currentTail));
        return false;
    }
    assert(messages.size() > currentHead && "TaskMessageQueue::push currentHead out of range");
    messages.at(currentHead) = msg;
    head.store(nextHead, std::memory_order_release);
    return true;
}

template<typename MsgType>
bool TaskMessageQueue<MsgType>::pop(MsgType& msg){
    size_t currentTail = tail.load(std::memory_order_relaxed);
    size_t currentHead = head.load(std::memory_order_acquire);
    if(currentTail == currentHead)
        return false;
    assert(messages.size() > currentTail && "TaskMessageQueue::pop currentTail out of range");
    msg = messages.at(currentTail);
    tail.store((currentTail + 1) % capacity, std::memory_order_release);
    return true;
}

template<typename ParentType, typename MsgType>
TaskWrapper<ParentType, MsgType>::TaskWrapper(ParentType* object, int priority, std::string name)
        :object(object),
        priority(priority),
        name(name)
{
    assert(object != nullptr && "TaskWrapper constructed with null object");
    auxiliaryTask = Bela_createAuxiliaryTask(taskWorkMessages, this->priority, this->name.c_str(), this);
    if(!auxiliaryTask){
        throw std::runtime_error("TaskWrapper: failed to create auxiliary task");
    }
    DEBUG_RT_PRINTF("TaskWrapper ctor name=%s prio=%d this=%p\n", this->name.c_str(), this->priority, this);
}

template<typename ParentType, typename MsgType>
void TaskWrapper<ParentType, MsgType>::setScheduleFlag(){
    if(!isInFlight()) needsScheduling = true;
}

template<typename ParentType, typename MsgType>
bool TaskWrapper<ParentType, MsgType>::isInFlight(){
    return taskInFlight.load(std::memory_order_acquire);
}

template<typename ParentType, typename MsgType>
bool TaskWrapper<ParentType, MsgType>::tryClaimInFlight(){
    bool expected = false;
    return taskInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
}

template<typename ParentType, typename MsgType>
void TaskWrapper<ParentType, MsgType>::taskCheckAndWorkMessages(){
    if(needsScheduling){
        if(tryClaimInFlight()){
            int scheduleResponse = Bela_scheduleAuxiliaryTask(auxiliaryTask);
            if(scheduleResponse != 0){
                taskInFlight.store(false, std::memory_order_release);
                if(scheduleResponse == 0) DEBUG_RT_PRINTF("successfully schedulded name=%s\n", name.c_str());
                if(scheduleResponse != EBUSY){
                    if(scheduleResponse == EINVAL){
                        DEBUG_RT_PRINTF("TaskWrapper::processBlockwise(): Bela_scheduleAuxiliaryTask returned EINVAL so prbl sth with the init failed name=%s \n",
                            this->name.c_str());
                    } else {
                        std::string& localName = this->getName();
                        DEBUG_RT_PRINTF("TaskWrapper::processBlockwise(): Bela_scheduleAuxiliaryTask error=%d name=%s \n",
                        scheduleResponse, localName.c_str());
                    }
                }

            }
            needsScheduling = false;
        }
        //if tryClaim fails the needsschedule probably is already worked. still the needsschedule stays true and the
        //task is scheduled another time after it is finished just in case but on that schedule it will probably do nothing
    } 
}


template<typename ParentType, typename MsgType>
bool TaskWrapper<ParentType, MsgType>::tryReleaseInFlight(){
    bool expected = true;
    return taskInFlight.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
}

template<typename ParentType, typename MsgType>
std::string& TaskWrapper<ParentType, MsgType>::getName(){
    return name;
}

template<typename ParentType, typename MsgType>
bool TaskWrapper<ParentType, MsgType>::pushMessage(TaskMessageTarget target, MsgType msg){
    if(target == TaskMessageTarget::AudioThread){
        return taskToAudio.push(msg);
    }
    if(target == TaskMessageTarget::TaskThread){
        bool ok = audioToTask.push(msg);
        if(!needsScheduling) this->setScheduleFlag();
        return ok;
    }
    throw std::runtime_error("TaskWrapper::pushMessage called with invalid target");
}

template<typename ParentType, typename MsgType>
bool TaskWrapper<ParentType, MsgType>::popMessage(TaskMessageTarget target, MsgType& msg){
    if(target == TaskMessageTarget::AudioThread){
        return taskToAudio.pop(msg);
    }
    if(target == TaskMessageTarget::TaskThread){
        return audioToTask.pop(msg);
    }
    throw std::runtime_error("TaskWrapper::popMessage called with invalid target");
    return false;
}

template<typename ParentType, typename MsgType>
void TaskWrapper<ParentType, MsgType>::taskWorkMessages(void* arg) {
    TaskWrapper<ParentType, MsgType>* taskWrapper = static_cast<TaskWrapper<ParentType, MsgType>*>(arg);
    MsgType msg;
    while(taskWrapper->popMessage(TaskMessageTarget::TaskThread, msg)){
        taskWrapper->object->taskWorkMessage(taskWrapper->name, msg);
    }
    if(!taskWrapper->tryReleaseInFlight()){
        DEBUG_PRINTF("Error try release in flight failed");
    }
}

// Explicit instantiations.
template class TaskWrapper<DisplayContextReal, DisplayMessage>;
template class TaskWrapper<AudioStreamer, StreamingMessage>;
template class TaskWrapper<StreamingBuffer, StreamingMessage>;
template class TaskWrapper<SamplePack, DefaultTaskMessage>;