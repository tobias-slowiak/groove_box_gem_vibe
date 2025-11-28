#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>
//
#include <stdexcept>
#include <exception>
#include <dirent.h>
#include <algorithm>
#include <cassert>

#include "../include/StreamingBuffer.h"
#include "../include/DebugLog.h"
#include "../include/ResourceManager.h"



//TODO: still missing a way to write to the sd card for storing loops and samples.

void streamSamplesOnThread(void* inArg){
    printf("\n\nstreamSamplesOnThread fct \n\n");
	StreamingBuffer* buffer = static_cast<StreamingBuffer*>(inArg);
	buffer->streamSamples();
}

std::vector<std::string> listWavFiles(const std::string folderPath) {
    //printf("Listing WAV files in folder: %s\n", folderPath.c_str());
    std::vector<std::string> result;
    DIR* dir = opendir(folderPath.c_str());
    if (dir == nullptr) return result;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".wav") {
            result.push_back(folderPath + "/" + name);
        }
    }
    //printf("Found WAV files: %d\n", result.size());

    closedir(dir);
    return result;
}


StreamingBuffer::StreamingBuffer(ResourceManager* resourceManager,
    size_t size, std::string bufferName,std::string folderPath)
        : resourceManager(resourceManager), bufferName(bufferName), folderPath(folderPath){
    assert(resourceManager != nullptr);
    assert(size > 0);
    
    buffer.resize(size, 0.0f);
    DEBUG_RT_PRINTF("creating buffer with size %d\n", buffer.size());
    std::string samplesTaskName = "SB_streamTask_" + bufferName;
    streamSamplesTask = Bela_createAuxiliaryTask(streamSamplesOnThread, sampleStreamPrio, samplesTaskName.c_str(), (void*)this);
    
    //do the first stream of starts at setup on regular setup thread.
    this->streamStarts();
    DEBUG_RT_PRINTF("Finished setting up streaming buffer %s", bufferName.c_str());
    this->printInfo();
}

void StreamingBuffer::initForFolder(std::string folderPath, size_t chunkLength){

    this->chunkLength = chunkLength;
    this->folderPath = folderPath;

    pendingSamplesToLoad.clear();
    streamStartsNeedsScheduling = true;
}


void StreamingBuffer::eraseSample(std::pair<int,int> sampleIdentifier){
    //mark all chunks except the start chunk as free and remove them from chunkStartIndices
    auto& sampleChunkStarts = chunkStartIndices[sampleIdentifier];
    while (sampleChunkStarts.size() > 1) {
        assert(sampleChunkStarts.size() > 1);
        size_t chunkStartIndex = sampleChunkStarts.at(1);
        size_t globalChunkIndex = (chunkStartIndex - streamedChunkAreaStartIndex) / chunkLength;
        assert(streamedChunkOwnership.size() > globalChunkIndex);
        sampleChunkStarts.erase(sampleChunkStarts.begin() + 1);
        streamedChunkOwnership[globalChunkIndex] = {-1, -1};
    }
    //remove it from fullyLoadedSamples
    fullyLoadedSamples.erase(std::remove(fullyLoadedSamples.begin(), fullyLoadedSamples.end(), sampleIdentifier), fullyLoadedSamples.end());
}

size_t StreamingBuffer::requestSample(std::pair<int,int> sampleIdentifier){
    currentRequestId++;
    if(currentRequestId == SIZE_MAX){
        currentRequestId = 1;
    }

    activeRequests[currentRequestId] = Request(resourceManager, sampleIdentifier, this, currentRequestId);
    if(std::find(fullyLoadedSamples.begin(), fullyLoadedSamples.end(), sampleIdentifier) != fullyLoadedSamples.end()){
        DEBUG_RT_PRINTF("StreamingBuffer::requestSample: sample for sampleId: %d %d already fully loaded\n", sampleIdentifier.first, sampleIdentifier.second);
    } else {
        DEBUG_RT_PRINTF("StreamingBuffer::requestSample: sampleId: %d %d with requestId: %zu starting load\n", sampleIdentifier.first, sampleIdentifier.second, currentRequestId);
        pendingSamplesToLoad.push_back(sampleIdentifier);
        streamChunksNeedsScheduling = true;
    }
    return currentRequestId;
}

void StreamingBuffer::processBlockwise(){
    assert(resourceManager != nullptr);
    static int blocksTillNextPrint = 0;
    if(streamStartsNeedsScheduling){
        auto status = scheduleStreamTask(StreamJobKind::Starts);
        if(status == ScheduleStatus::Scheduled){
            DEBUG_RT_PRINTF("Invoked stream for sampleStarts\n");
            streamStartsNeedsScheduling = false;
        } else if(status == ScheduleStatus::Busy){
            if(blocksTillNextPrint%1000 == 0){
                DEBUG_RT_PRINTF("failed invoke stream for sampleStarts try on next block, print again in 1000 blocks\n");
            }
            blocksTillNextPrint += 1;
        } else {
            DEBUG_RT_PRINTF("scheduleStreamTask (starts version) failed, investigate!\n");
        }
    }
    if(streamChunksNeedsScheduling){
        auto currentJob = jobInFlight.load(std::memory_order_acquire);
        if(currentJob == StreamJobKind::Starts){
            if(blocksTillNextPrint%1000 == 0)
                DEBUG_RT_PRINTF("failed invoke stream for chunks, because still loading starts. try on next block, print again in 1000 blocks\n");
            blocksTillNextPrint += 1;
            return;
        }
        if(currentJob == StreamJobKind::Chunks){
            // Already streaming chunks; the pending load will be handled by the in-flight job.
            streamChunksNeedsScheduling = false;
            return;
        } else {
            auto status = scheduleStreamTask(StreamJobKind::Chunks);
            if(status == ScheduleStatus::Scheduled){
                DEBUG_RT_PRINTF("Invoked stream for chunks\n");
                streamChunksNeedsScheduling = false;
            } else if(status == ScheduleStatus::Busy){
                DEBUG_RT_PRINTF("StreamingBuffer::processBlockwise: worker busy, will retry chunk scheduling\n");
            } else {
                DEBUG_RT_PRINTF("scheduleStreamTask (chunk version) failed, investigate!\n");
            }
        }
    }
}

StreamingBuffer::ScheduleStatus StreamingBuffer::scheduleStreamTask(StreamJobKind kind){
    StreamJobKind expected = StreamJobKind::None;
    if(!jobInFlight.compare_exchange_strong(expected, kind, std::memory_order_acq_rel)){
        return ScheduleStatus::Busy;
    }
    uint32_t jobId = 0;
    if(kind == StreamJobKind::Starts){
        jobId = startJobsIssued.fetch_add(1, std::memory_order_relaxed) + 1;
        activeStartJobId.store(jobId, std::memory_order_release);
        DEBUG_RT_PRINTF("Scheduling stream task for starts with Id %d\n", jobId);
    } else {
        jobId = chunkJobsIssued.fetch_add(1, std::memory_order_relaxed) + 1;
        activeChunkJobId.store(jobId, std::memory_order_release);
    }
    int scheduleReturn = Bela_scheduleAuxiliaryTask(streamSamplesTask);
    if(scheduleReturn == 0){
        return ScheduleStatus::Scheduled;
    }
    // Roll back counters if scheduling failed.
    if(kind == StreamJobKind::Starts){
        activeStartJobId.store(0, std::memory_order_release);
        startJobsIssued.fetch_sub(1, std::memory_order_relaxed);
    } else {
        activeChunkJobId.store(0, std::memory_order_release);
        chunkJobsIssued.fetch_sub(1, std::memory_order_relaxed);
    }
    jobInFlight.store(StreamJobKind::None, std::memory_order_release);

    if(scheduleReturn == EBUSY){
        return ScheduleStatus::Busy;
    }
    DEBUG_RT_PRINTF("Bela_scheduleAuxiliaryTask returned error %d\n", scheduleReturn);
    return ScheduleStatus::Error;
}

Request::Request(ResourceManager* resourceManager, std::pair<int,int> sampleIdentifier, StreamingBuffer* buffer,
                size_t requestId):
                requestId(requestId), resourceManager(resourceManager),
                sampleIdentifier(sampleIdentifier), buffer(buffer),
                chunkStartIndices(&(buffer->chunkStartIndices[sampleIdentifier])),
                sampleLength(buffer->sampleLengths[sampleIdentifier]),
                chunkLength(buffer->chunkLength)
    {
        assert(resourceManager != nullptr);
        assert(buffer != nullptr);
        assert(chunkStartIndices != nullptr);
        assert(!chunkStartIndices->empty());
        chunkStartIndex = chunkStartIndices->at(0);
        if(chunkStartIndices->size() > 0)
            DEBUG_RT_PRINTF("chunkstartindices size: %zu\n", chunkStartIndices->size());
        else
            DEBUG_RT_PRINTF("Request: sample %d %d chunkStartIndices empty\n", sampleIdentifier.first, sampleIdentifier.second);
    }

float Request::getNextSample(){
    assert(resourceManager != nullptr);
    assert(buffer != nullptr);
    assert(chunkStartIndices != nullptr);
    readIndexInChunk++;
    if(chunkIndex * chunkLength + readIndexInChunk > sampleLength){
        //end of sample reached.
        return resourceManager->END_OF_SAMPLE;
    }
    if(readIndexInChunk >= chunkLength){
        readIndexInChunk = 0;
        chunkIndex++;
        if(chunkIndex >= chunkStartIndices->size()){
            std::string errormsg = "Request::getNextSample: request: "
                    + std::to_string(requestId) + " chunkIndex " + std::to_string(chunkIndex)
                    + " not yet loaded. Stream too slow.";
            throw std::out_of_range(errormsg.c_str());
        }
        chunkStartIndex = chunkStartIndices->at(chunkIndex);
    }
    size_t absoluteIndex = chunkStartIndex + readIndexInChunk;
    assert(buffer->buffer.size() > absoluteIndex);
    return buffer->at(absoluteIndex);
}

float StreamingBuffer::getNextSample(size_t requestId){
    if(requestId == 0){
        DEBUG_RT_PRINTF("ERROR: requesting RequestId 0. This should never happen. The request had to wait because the thread was still loading the starts.");
        throw std::runtime_error("ERROR: requesting RequestId 0. This should never happen. The request had to wait because the thread was still loading the starts.");
    }
    if(activeRequests.find(requestId) == activeRequests.end()) return 0.0f;
    float nextSample = activeRequests[requestId].getNextSample();
    if(nextSample == resourceManager->END_OF_SAMPLE){
        activeRequests.erase(requestId);
    }
    return nextSample;
}

int StreamingBuffer::getRequestSampleLength(size_t requestId){
    return sampleLengths[activeRequests[requestId].getSampleIdentifier()];
}

void StreamingBuffer::streamStarts(){
    DEBUG_PRINTF("\n\n\nstarting stream (starts version)\n");
    this->clearContainers();
    //----------------------------------------------------------------------------
    // Make a list of available samples in the folder
    availableSamples.clear();
    std::vector<std::string> wavFiles = listWavFiles(folderPath);
    // TODO: make this for auto filename in wavfiles.
    // Also make it not key and velocity because it also could be other sample identifiers.
    for(int key = 0; key < 128; key++){
        for(int velocity = 0; velocity < 128; velocity++){
            std::pair<int,int> sampleIdentifier = {key, velocity};
            std::string filename = folderPath + "/" + std::to_string(key) + "_" + std::to_string(velocity) + ".wav";
            if(std::find(wavFiles.begin(), wavFiles.end(), filename) != wavFiles.end()){
                filenames[sampleIdentifier] = filename;
                availableSamples.push_back(sampleIdentifier);
            }
        }
    }
    //----------------------------------------------------------------------------

    //----------------------------------------------------------------------------
    // Make 2 areas (startArea and streamedChunkArea) in the bigSamplePackBuffer
    // startArea has #availableSamples * chunkLength samples that are only loaded once at initForFolder
    // For convenience it's used size is fit to numStarts * chunkLength + numChunks * chunkLength
    streamedChunkAreaStartIndex = availableSamples.size() * chunkLength;
    size_t streamedChunkAreaLength = buffer.size() - streamedChunkAreaStartIndex;
    size_t numChunks = streamedChunkAreaLength / chunkLength;
    //DEBUG_PRINTF("streamingBuffer initialisation: chunk area can hold %zu chunks\n", numChunks);
    availableBufferLength = availableSamples.size() * chunkLength + numChunks * chunkLength;
    //DEBUG_PRINTF("streamingBuffer initialisation: availableBufferLength set to %zu samples == %zu chunks\n", availableBufferLength, availableBufferLength / chunkLength);
    streamedChunkOwnership.clear();
    for(int i = 0; i < numChunks; i++){
        streamedChunkOwnership.push_back({-1, -1}); // mark as free
    }
    //----------------------------------------------------------------------------

    size_t currentSampleStartIndex = 0;
    // Load sample starts into big buffer
    for(auto& sampleIdentifier: availableSamples){
        std::string filename = folderPath + "/" + std::to_string(sampleIdentifier.first) + "_" + std::to_string(sampleIdentifier.second) + ".wav";
        filenames[sampleIdentifier] = filename;
        //TODO: check if sample is shorter than chunkLength
        int channel = 0; //TODO: make this multi channel capable (stereo to mono)
        sampleLengths[sampleIdentifier] = AudioFileUtilities::getNumFrames(filename);
        assert(buffer.size() >= currentSampleStartIndex + chunkLength);
        if(AudioFileUtilities::getSamples(filename, buffer.data() + currentSampleStartIndex, channel, 0, chunkLength) == 0) {
            //DEBUG_PRINTF("%d %d, ", sampleIdentifier.first, sampleIdentifier.second);
        } else {
            DEBUG_PRINTF("failed to load sample %s\n", filename.c_str());
            throw std::runtime_error("StreamingBuffer::initForFolder: failed to load sample");
        }
        chunkStartIndices[sampleIdentifier].push_back(currentSampleStartIndex);
        currentSampleStartIndex += chunkLength;
    }
}

void StreamingBuffer::streamChunks(){
    DEBUG_PRINTF("\n\n\nstarting stream (chunk version)\n");
    //DEBUG_PRINTF("available samples: %zu\n\n", availableSamples.size());
    //TODO: this seems dangerous in case the audio thread is adding something to pending samples while the stream is ongoing.
    std::map<std::pair<int,int>, size_t> currentChunkToLoad;
    while(pendingSamplesToLoad.size() > 0){
        /*
        //old debug print
        DEBUG_PRINTF("\n working on pending samples\n");
        DEBUG_PRINTF("Pending Samples:\n");
        for(auto& sid: pendingSamplesToLoad){
            DEBUG_PRINTF("%d %d, ", sid.first, sid.second);
        }
        DEBUG_PRINTF("\n\n\n");
        //DEBUG_PRINTF("sample Id: %d %d\n\n", pendingSamplesToLoad.at(0).first, pendingSamplesToLoad.at(0).second);
        */
        for(int i = 0; i < pendingSamplesToLoad.size(); i++){
            std::pair<int, int> sampleIdentifier = {-1,-1};
            assert(i >= 0 && pendingSamplesToLoad.size() > static_cast<size_t>(i));
            sampleIdentifier = pendingSamplesToLoad.at(i);

            //see if i am already loading this sample
            if(currentChunkToLoad.find(sampleIdentifier) == currentChunkToLoad.end()){
                currentChunkToLoad[sampleIdentifier] = 1;
                DEBUG_PRINTF("starting to load the first chunk\n");
            }
            
            //------------MANAGING THE READ FROM SD CARD --------------------------------
            size_t sdFileReadStartIndex = currentChunkToLoad[sampleIdentifier] * chunkLength;
            size_t sdFileReadEndIndex = sdFileReadStartIndex + chunkLength;

            //check if this is the last chunk to load for this sample
            if((currentChunkToLoad[sampleIdentifier] + 1) * chunkLength >= sampleLengths[sampleIdentifier]){ // ending within this chunk
                DEBUG_PRINTF("loading the last chunk of %d %d\n", sampleIdentifier.first, sampleIdentifier.second);
                sdFileReadEndIndex = sampleLengths[sampleIdentifier];
                
                //sample fully loaded
                fullyLoadedSamples.push_back(sampleIdentifier);
                pendingSamplesToLoad.erase(pendingSamplesToLoad.begin() + i);
                currentChunkToLoad.erase(sampleIdentifier);
                i--;
            } else {
                currentChunkToLoad[sampleIdentifier]++;
            }

            //----------------MANAGING THE WRITE TO THE BUFFER-------------------------
                        bool freeChunkFound = false;
            size_t currentSampleStartIndex = streamedChunkAreaStartIndex;
            int targetChunkIndex = -1;
            std::pair<int,int> sampleToEvict = {-1,-1};

            auto requestProgress = [&](const Request& req){
                return req.chunkIndex * chunkLength + req.readIndexInChunk;
            };

            // Pass 1: grab the first free slot.
            for(size_t globalChunkIndex = 0; globalChunkIndex < streamedChunkOwnership.size(); ++globalChunkIndex){
                assert(streamedChunkOwnership.size() > globalChunkIndex);
                if(streamedChunkOwnership.at(globalChunkIndex).first == -1){
                    targetChunkIndex = static_cast<int>(globalChunkIndex);
                    freeChunkFound = true;
                    break;
                }
            }

            
            // Pass 2: reuse a chunk whose sample is no longer requested.
            if(!freeChunkFound){
                for(size_t globalChunkIndex = 0; globalChunkIndex < streamedChunkOwnership.size(); ++globalChunkIndex){
                    assert(streamedChunkOwnership.size() > globalChunkIndex);
                    std::pair<int,int> owner = streamedChunkOwnership.at(globalChunkIndex);
                    if(owner.first == -1){
                        continue;
                    }
                    bool sampleStillActive = false;
                    for(const auto& entry : activeRequests){
                        if(entry.second.sampleIdentifier == owner){
                            sampleStillActive = true;
                            break;
                        }
                    }
                    if(!sampleStillActive){
                        sampleToEvict = owner;
                        targetChunkIndex = static_cast<int>(globalChunkIndex);
                        freeChunkFound = true;
                        break;
                    }
                }
            }

            // Pass 3: voice stealing – drop the request that has been running the longest.
            if(!freeChunkFound){
                size_t maxProgress = 0;
                std::pair<int,int> victimSample = {-1,-1};
                for(const auto& entry : activeRequests){
                    // Optionally skip the sample we are currently extending; remove this check if you prefer.
                    if(entry.second.sampleIdentifier == sampleIdentifier){
                        continue;
                    }
                    size_t progress = requestProgress(entry.second);
                    if(progress >= maxProgress){
                        maxProgress = progress;
                        victimSample = entry.second.sampleIdentifier;
                    }
                }
                if(victimSample.first == -1){
                    // All active voices point to the same sample we’re extending; fall back to stealing that one.
                    for(const auto& entry : activeRequests){
                        size_t progress = requestProgress(entry.second);
                        if(progress >= maxProgress){
                            maxProgress = progress;
                            victimSample = entry.second.sampleIdentifier;
                        }
                    }
                }
                if(victimSample.first != -1){
                    sampleToEvict = victimSample;

                    // Remove every request tied to the victim sample so no reader touches freed memory.
                    for(auto it = activeRequests.begin(); it != activeRequests.end(); ){
                        if(it->second.sampleIdentifier == victimSample){
                            it = activeRequests.erase(it);
                        } else {
                            ++it;
                        }
                    }

                    // Reuse the first chunk owned by that sample.
                    for(size_t globalChunkIndex = 0; globalChunkIndex < streamedChunkOwnership.size(); ++globalChunkIndex){
                        assert(streamedChunkOwnership.size() > globalChunkIndex);
                        if(streamedChunkOwnership.at(globalChunkIndex) == victimSample){
                            targetChunkIndex = static_cast<int>(globalChunkIndex);
                            freeChunkFound = true;
                            break;
                        }
                    }
                }
            }

            if(freeChunkFound){
                if(sampleToEvict.first != -1){
                    this->eraseSample(sampleToEvict);
                }
                assert(targetChunkIndex >= 0);
                size_t chunkOffset = static_cast<size_t>(targetChunkIndex) * chunkLength;
                currentSampleStartIndex += chunkOffset;
                assert(buffer.size() >= currentSampleStartIndex + chunkLength);
                chunkStartIndices[sampleIdentifier].push_back(currentSampleStartIndex);
                assert(streamedChunkOwnership.size() > static_cast<size_t>(targetChunkIndex));
                streamedChunkOwnership.at(static_cast<size_t>(targetChunkIndex)) = sampleIdentifier;
            } else {
                DEBUG_PRINTF("StreamingBuffer::streamSamples: no free chunk found to load sample for sampleId1 %d sampleId2 %d. buffer too small.\n", sampleIdentifier.first, sampleIdentifier.second);
                throw std::runtime_error("StreamingBuffer::streamSamples: no free chunk found. buffer too small.");
            }


            //--------------------DO THE ACTUAL STREAM OF THE CHUNK----------------------
            //TODO: make stereo to mono as above.
            int channel = 0;
            //DEBUG_PRINTF("loading chunk from sdcardindex %d\n", sdFileReadEndIndex);
            assert(buffer.size() >= currentSampleStartIndex + chunkLength);
            if(AudioFileUtilities::getSamples(filenames[sampleIdentifier], buffer.data() + currentSampleStartIndex, channel, sdFileReadStartIndex, sdFileReadEndIndex) == 0) {
                //DEBUG_PRINTF("loaded chunk at bufferId: %zu from sdcardfileId: %zu  %s\n", currentSampleStartIndex, sdFileReadStartIndex,  filenames[sampleIdentifier].c_str());
            } else {
                DEBUG_PRINTF("failed to load sample %s\n", filenames[sampleIdentifier].c_str());
                throw std::runtime_error("StreamingBuffer::initForFolder: failed to load sample");
            }
        }
    }
}

void StreamingBuffer::streamSamples(){
    //TODO: eliminate race condition when starts were just loaded and immediately a request is made.
    DEBUG_PRINTF("stream on thread----------------------\n");
    assert(resourceManager != nullptr);

    auto finishJob = [this](StreamJobKind kind, bool markCompleted){
        if(kind == StreamJobKind::Starts){
            const uint32_t jobId = activeStartJobId.exchange(0, std::memory_order_acq_rel);
            if(markCompleted && jobId != 0){
                startJobsCompleted.store(jobId, std::memory_order_release);
            }
        } else if(kind == StreamJobKind::Chunks){
            const uint32_t jobId = activeChunkJobId.exchange(0, std::memory_order_acq_rel);
            if(markCompleted && jobId != 0){
                chunkJobsCompleted.store(jobId, std::memory_order_release);
            }
        }
    };

    StreamJobKind job = jobInFlight.load(std::memory_order_acquire);

    auto clearJobInFlight = [this](){
        jobInFlight.store(StreamJobKind::None, std::memory_order_release);
    };

    try{
        switch(job){
            case StreamJobKind::Starts: {
                streamStarts();
                finishJob(StreamJobKind::Starts, true);
                break;
            }
            case StreamJobKind::Chunks: {
                streamChunks();
                finishJob(StreamJobKind::Chunks, true);
                break;
            }
            case StreamJobKind::None:
            default:
                printf("streamSamples invoked without a pending job\n");
                break;
        }
    } catch(const std::exception& e){
        printf("streamSamples exception: %s\n", e.what());
        finishJob(job, true);
    } catch(...){
        printf("streamSamples exception: unknown error\n");
        finishJob(job, true);
    }

    clearJobInFlight();
    printf("stream on thread complete ----------------------\n");
    return;
}

void StreamingBuffer::clearStreamingChunks(){
    for(auto& sampleIdentifier: streamedChunkOwnership){
        sampleIdentifier = {-1,-1};
    }
    fullyLoadedSamples.clear();
}

void StreamingBuffer::clearContainers(){
    filenames.clear();
    availableSamples.clear();
    pendingSamplesToLoad.clear();
    fullyLoadedSamples.clear();
    sampleLengths.clear();
    streamedChunkOwnership.clear();
    chunkStartIndices.clear();
    activeRequests.clear();
    sdCardReadIndices.clear();
}

void StreamingBuffer::printInfo() const {
    printf("StreamingBuffer '%s' info\n", bufferName.c_str());
    printf("  folder: %s\n", folderPath.c_str());
    printf("  buffer size: %zu samples\n", buffer.size());
    printf("  chunk length: %zu samples\n", chunkLength);
    printf("  availableBufferLength: %zu\n", availableBufferLength);
    printf("  streamedChunkAreaStartIndex: %zu\n", streamedChunkAreaStartIndex);
    auto job = jobInFlight.load(std::memory_order_acquire);
    const char* jobLabel = "None";
    if(job == StreamJobKind::Starts){
        jobLabel = "Starts";
    } else if(job == StreamJobKind::Chunks){
        jobLabel = "Chunks";
    }
    printf("  jobInFlight: %s\n", jobLabel);
    printf("  startJobs issued/completed: %u / %u\n",
        getStartJobsIssued(), getStartJobsCompleted());
    printf("  chunkJobs issued/completed: %u / %u\n",
        getChunkJobsIssued(), getChunkJobsCompleted());
    printf("  currentRequestId: %zu\n", currentRequestId);
    printf("  pendingSamplesToLoad: %zu\n", pendingSamplesToLoad.size());
    printf("  fullyLoadedSamples: %zu\n", fullyLoadedSamples.size());
    printf("  activeRequests: %zu\n", activeRequests.size());

    auto printSampleVector = [](const char* label, const std::vector<std::pair<int, int>>& items) {
        printf("  %s (showing up to 5):\n", label);
        size_t count = items.size() < 5 ? items.size() : 5;
        for (size_t i = 0; i < count; ++i) {
            printf("    [%zu] (%d, %d)\n", i, items[i].first, items[i].second);
        }
        if (items.size() > count) {
            printf("    ... (%zu more)\n", items.size() - count);
        }
    };

    printSampleVector("availableSamples", availableSamples);
    printSampleVector("pendingSamplesToLoad", pendingSamplesToLoad);
    printSampleVector("fullyLoadedSamples", fullyLoadedSamples);

    printf("  filenames (showing up to 5):\n");
    size_t filenamesShown = 0;
    for (const auto& entry : filenames) {
        printf("    (%d, %d) -> %s\n", entry.first.first, entry.first.second, entry.second.c_str());
        if (++filenamesShown >= 5) {
            break;
        }
    }
    if (filenames.size() > filenamesShown) {
        printf("    ... (%zu more)\n", filenames.size() - filenamesShown);
    }

    printf("  sampleLengths (showing up to 5):\n");
    size_t sampleLengthsShown = 0;
    for (const auto& entry : sampleLengths) {
        printf("    (%d, %d) -> %zu\n", entry.first.first, entry.first.second, entry.second);
        if (++sampleLengthsShown >= 5) {
            break;
        }
    }
    if (sampleLengths.size() > sampleLengthsShown) {
        printf("    ... (%zu more)\n", sampleLengths.size() - sampleLengthsShown);
    }

    printf("  streamedChunkOwnership (showing up to 5):\n");
    size_t chunkOwnershipCount = streamedChunkOwnership.size() < 5 ? streamedChunkOwnership.size() : 5;
    for (size_t i = 0; i < chunkOwnershipCount; ++i) {
        printf("    [%zu] (%d, %d)\n", i, streamedChunkOwnership[i].first, streamedChunkOwnership[i].second);
    }
    if (streamedChunkOwnership.size() > chunkOwnershipCount) {
        printf("    ... (%zu more)\n", streamedChunkOwnership.size() - chunkOwnershipCount);
    }

    printf("  chunkStartIndices (showing up to 5 entries):\n");
    size_t chunkStartIndicesShown = 0;
    for (const auto& entry : chunkStartIndices) {
        printf("    (%d, %d):", entry.first.first, entry.first.second);
        size_t indicesToShow = entry.second.size() < 5 ? entry.second.size() : 5;
        for (size_t i = 0; i < indicesToShow; ++i) {
            printf(" %zu", entry.second[i]);
        }
        if (entry.second.size() > indicesToShow) {
            printf(" ...");
        }
        printf("\n");
        if (++chunkStartIndicesShown >= 5) {
            break;
        }
    }
    if (chunkStartIndices.size() > chunkStartIndicesShown) {
        printf("    ... (%zu more)\n", chunkStartIndices.size() - chunkStartIndicesShown);
    }

    printf("  sdCardReadIndices (showing up to 5):\n");
    size_t sdCardIndicesShown = 0;
    for (const auto& entry : sdCardReadIndices) {
        printf("    (%d, %d) -> %zu\n", entry.first.first, entry.first.second, entry.second);
        if (++sdCardIndicesShown >= 5) {
            break;
        }
    }
    if (sdCardReadIndices.size() > sdCardIndicesShown) {
        printf("    ... (%zu more)\n", sdCardReadIndices.size() - sdCardIndicesShown);
    }

    printf("  activeRequests (showing up to 5):\n");
    size_t requestsShown = 0;
    for (const auto& entry : activeRequests) {
        printf("    request %zu -> (%d, %d)\n", entry.first, entry.second.sampleIdentifier.first, entry.second.sampleIdentifier.second);
        if (++requestsShown >= 5) {
            break;
        }
    }
    if (activeRequests.size() > requestsShown) {
        printf("    ... (%zu more)\n", activeRequests.size() - requestsShown);
    }
}

bool StreamingBuffer::isStreaming() const {
    return jobInFlight.load(std::memory_order_acquire) != StreamJobKind::None;
}

uint32_t StreamingBuffer::getStartJobsIssued() const {
    return startJobsIssued.load(std::memory_order_acquire);
}

uint32_t StreamingBuffer::getStartJobsCompleted() const {
    return startJobsCompleted.load(std::memory_order_acquire);
}

uint32_t StreamingBuffer::getChunkJobsIssued() const {
    return chunkJobsIssued.load(std::memory_order_acquire);
}

uint32_t StreamingBuffer::getChunkJobsCompleted() const {
    return chunkJobsCompleted.load(std::memory_order_acquire);
}
