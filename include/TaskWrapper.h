#pragma once
#include <Bela.h>
#include <string>
#include <atomic>
#include <stdexcept>
#include <vector>
#include <cassert>

static constexpr size_t DEFAULT_TASK_MESSAGE_QUEUE_CAPACITY = 512;

enum class TaskMessageTarget: int{
    AudioThread,
    TaskThread
};

struct TaskMessage {
    int Id1;
    int Id2;
    int Id3;
    int Id4;
    int Id5;
};

class TaskMessageQueue {
public:

    TaskMessageQueue() {
        assert(0 > 1  && "TaskMessageQueue default ctor should never trigger.");}
    
    TaskMessageQueue(size_t capacity): capacity(capacity){
        assert(capacity > 2 && "TaskMessageQueue capacity must be > 2");
        messages.resize(capacity);
    }
    
    bool push(TaskMessage value);

    bool pop(TaskMessage& out);

private:
    size_t capacity;
    std::vector<TaskMessage> messages;
    std::atomic<size_t> tail{0};
    std::atomic<size_t> head{0}; // producer-only
};

template<typename T>
class TaskWrapper{
public:
    TaskWrapper(T* object, int priority, std::string name)
        :object(object),
        priority(priority),
        name(name)
    {
        auxiliaryTask = Bela_createAuxiliaryTask(genericTaskFunction, priority, name.c_str(), this);
    }

    void setScheduleFlag(){needsScheduling = true;}

    bool inFlight(){
        bool expected = false;
        return taskInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
    }

    void processBlockwise(){
        if(needsScheduling && !inFlight()){
            int scheduleResponse = Bela_scheduleAuxiliaryTask(auxiliaryTask);
            if(scheduleResponse != 0){
                taskInFlight.store(false, std::memory_order_release);
                if(scheduleResponse != EBUSY){
                    DEBUG_RT_PRINTF("TaskWrapper::processBlockwise(): Bela_scheduleAuxiliaryTask sent error code: %d on task %s\n",
                        scheduleResponse, name.c_str());
                }
            }
        }
    }

    void taskJob(std::string& taskName){
        object->taskJob(taskName);
    }

    void finished(){taskInFlight.store(false, std::memory_order_release);}

    const std::string& getName(){
        return name;
    }

    bool pushMessage(TaskMessageTarget target, TaskMessage taskMsg){
        if(target == TaskMessageTarget::AudioThread){
            return taskToAudio.push(taskMsg);
        }
        if(target == TaskMessageTarget::TaskThread){
            return audioToTask.push(taskMsg);
        }
        throw std::runtime_error("TaskWrapper::pushMessage called with invalid target");
        return false;
    }

    bool popMessage(TaskMessageTarget target, TaskMessage& taskMsg){
        if(target == TaskMessageTarget::AudioThread){
            return taskToAudio.pop(taskMsg);
        }
        if(target == TaskMessageTarget::TaskThread){
            return audioToTask.pop(taskMsg);
        }
        throw std::runtime_error("TaskWrapper::popMessage called with invalid target");
        return false;
    }

private:
    T* object;
    AuxiliaryTask auxiliaryTask;
    TaskMessageQueue audioToTask{DEFAULT_TASK_MESSAGE_QUEUE_CAPACITY};
    TaskMessageQueue taskToAudio{DEFAULT_TASK_MESSAGE_QUEUE_CAPACITY};
    std::atomic<bool> taskInFlight{false};
    int priority;
    std::string name;
    bool needsScheduling = false;

    static void genericTaskFunction(void* arg) {
        TaskWrapper* taskWrapper = static_cast<TaskWrapper<T>*>(arg);
        taskWrapper->taskJob(taskWrapper->getName());
        taskWrapper->finished();
    }
};
