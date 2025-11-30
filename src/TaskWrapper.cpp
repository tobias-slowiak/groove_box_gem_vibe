#include "../include/TaskWrapper.h"


///////////TaskMessageQueue///////////
bool TaskMessageQueue::push(TaskMessage msg) {
    size_t currentHead = head.load(std::memory_order_relaxed);
    size_t nextHead = (currentHead + 1) % capacity;
    if(nextHead == tail.load(std::memory_order_acquire)){
        throw std::runtime_error("TaskMessageQueue::push queue full, investigate!");
        return false;
    }
    assert(messages.size() > currentHead && "TaskMessageQueue::push currentHead out of range");
    messages.at(currentHead) = msg;
    head.store(nextHead, std::memory_order_release);
    return true;
}

bool TaskMessageQueue::pop(TaskMessage& msg) {
    size_t currentTail = tail.load(std::memory_order_relaxed);
    size_t currentHead = head.load(std::memory_order_acquire);
    if(currentTail == currentHead)
        return false;
    assert(messages.size() > currentTail && "TaskMessageQueue::pop currentTail out of range");
    msg = messages.at(currentTail);
    tail.store((currentTail + 1) % capacity, std::memory_order_release);
    return true;
}