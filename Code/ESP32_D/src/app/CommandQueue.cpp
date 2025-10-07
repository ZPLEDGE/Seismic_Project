#include "app/CommandQueue.h"

CommandQueue::CommandQueue(size_t queueSize) 
    : queueSize_(queueSize)
{
    queue_ = xQueueCreate(queueSize, sizeof(Command));
    if (queue_ == nullptr) {
        Serial.println("[CommandQueue] ERROR: Failed to create queue!");
    }
}

CommandQueue::~CommandQueue() {
    if (queue_ != nullptr) {
        vQueueDelete(queue_);
    }
}

bool CommandQueue::send(const Command& cmd, uint32_t timeoutMs) {
    if (queue_ == nullptr) {
        return false;
    }
    
    TickType_t ticks = (timeoutMs == 0) ? 0 : pdMS_TO_TICKS(timeoutMs);
    return xQueueSend(queue_, &cmd, ticks) == pdTRUE;
}

bool CommandQueue::receive(Command& cmd, uint32_t timeoutMs) {
    if (queue_ == nullptr) {
        return false;
    }
    
    TickType_t ticks = (timeoutMs == 0) ? 0 : pdMS_TO_TICKS(timeoutMs);
    return xQueueReceive(queue_, &cmd, ticks) == pdTRUE;
}

bool CommandQueue::hasCommands() const {
    if (queue_ == nullptr) {
        return false;
    }
    return uxQueueMessagesWaiting(queue_) > 0;
}

size_t CommandQueue::getCount() const {
    if (queue_ == nullptr) {
        return 0;
    }
    return uxQueueMessagesWaiting(queue_);
}

void CommandQueue::clear() {
    if (queue_ == nullptr) {
        return;
    }
    xQueueReset(queue_);
}