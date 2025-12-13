#pragma once

#include "../include/SampleIdentifier.h"

enum class StreamingMessageType : int {
    ChunkReady,
    InvalidatedChunk,
    StreamChunk,
    AssignChunk,
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
