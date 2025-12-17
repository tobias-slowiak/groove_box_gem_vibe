#pragma once

#include "../include/SampleIdentifier.h"

enum class StreamingMessageType : int {
    StreamChunk,
    InitializeSample,
    Clear,
    FlushInfo,
    FlushChunk,
    FlushComplete,
    releaseMutateOngoing
};

struct StreamingMessage{
    StreamingMessageType type;
    SampleIdentifier sampleIdentifier;
    int chunkIndex;
    int chunkIndexInBuffer;
};
