#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>

#include <stdexcept>
#include <dirent.h>
#include <algorithm>

#include "../include/StreamingBuffer.h"
#include "../include/DebugLog.h"
#include "../include/ResourceManager.h"



//TODO: still missing a way to write to the sd card for storing loops and samples.

void streamSamplesOnThread(void* inArg){
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
    while (chunkStartIndices[sampleIdentifier].size() > 1) {
        size_t chunkStartIndex = chunkStartIndices[sampleIdentifier].at(1);
        size_t globalChunkIndex = (chunkStartIndex - streamedChunkAreaStartIndex) / chunkLength;
        chunkStartIndices[sampleIdentifier].erase(chunkStartIndices[sampleIdentifier].begin() + 1);
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
    static int blocksTillNextPrint = 0;
    if(streamStartsNeedsScheduling){
        resourceManager->setStreamStartsFlag(true);
        int scheduleReturn = -1;
        scheduleReturn = Bela_scheduleAuxiliaryTask(streamSamplesTask);
        if(scheduleReturn == 0){
            DEBUG_RT_PRINTF("Invoked stream for sampleStarts\n");
            resourceManager->setStreamLoadingFlag(true);
            streamStartsNeedsScheduling = false;
        } else if(scheduleReturn == EBUSY) {
            if(blocksTillNextPrint%1000 == 0)
                DEBUG_RT_PRINTF("failed invoke stream for sampleStarts try on next block, print again in 1000 blocks\n");
            blocksTillNextPrint += 1;
        } else {
            DEBUG_RT_PRINTF("bela_scheduleauxiliarytask (starts version): returned error %d, investigate!\n", scheduleReturn);
        }
    }
    if(streamChunksNeedsScheduling){
        if(resourceManager->getStreamStartsFlag()){
            if(blocksTillNextPrint%1000 == 0)
                rt_printf("failed invoke stream for chunks, because still loading starts. try on next block, print again in 1000 blocks\n");
            blocksTillNextPrint += 1;
            return;
        } else {
            if(resourceManager->getStreamLoadingFlag()){
                //no need to invoke task, as it is already loading and will include the pending task.
                streamChunksNeedsScheduling = false;
                return;
            }
            int scheduleReturn = -1;
            scheduleReturn = Bela_scheduleAuxiliaryTask(streamSamplesTask);
            if(scheduleReturn == 0){
                rt_printf("Invoked stream for chunks\n");
                resourceManager->setStreamLoadingFlag(true);
                streamChunksNeedsScheduling = false;
            } else if(scheduleReturn == EBUSY){
                rt_printf("strange case: Bela_scheduleAuxiliaryTask returned EBUSY even though streamloadingflag and streamstartsflag are false\n");
            } else {
                rt_printf("bela_scheduleauxiliarytask: returned error %d, investigate!\n", scheduleReturn);
            }
        }
    }
}

float Request::getNextSample(){
    readIndexInChunk++;
    if(chunkIndex * buffer->chunkLength + readIndexInChunk > buffer->sampleLengths[sampleIdentifier]){
        //end of sample reached.
        return resourceManager->END_OF_SAMPLE;
    }
    if(readIndexInChunk >= buffer->chunkLength){
        readIndexInChunk = 0;
        chunkIndex++;
        if(chunkIndex >= buffer->chunkStartIndices[sampleIdentifier].size()){
            DEBUG_RT_PRINTF("request too slow: %zu", requestId);
            std::string errormsg = "Request::getNextSample: request: " + std::to_string(requestId) + " chunk not yet loaded. Stream too slow.";
            throw std::out_of_range(errormsg.c_str());
        }
        chunkStartIndex = buffer->chunkStartIndices[sampleIdentifier].at(chunkIndex);
    }
    return buffer->at(chunkStartIndex + readIndexInChunk);
}

float StreamingBuffer::getNextSample(size_t requestId){
    if(requestId == 0){
        rt_printf("ERROR: requesting RequestId 0. This should never happen. The request had to wait because the thread was still loading the starts.");
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
        if(AudioFileUtilities::getSamples(filename, buffer.data() + currentSampleStartIndex, channel, 0, chunkLength) == 0) {
            //DEBUG_PRINTF("%d %d, ", sampleIdentifier.first, sampleIdentifier.second);
        } else {
            DEBUG_PRINTF("failed to load sample %s\n", filename.c_str());
            throw std::runtime_error("StreamingBuffer::initForFolder: failed to load sample");
        }
        chunkStartIndices[sampleIdentifier].push_back(currentSampleStartIndex);
        currentSampleStartIndex += chunkLength;
    }
    resourceManager->setStreamStartsFlag(false);
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

            //check where there is free space in the streamedChunkArea
            for(int globalChunkIndex = 0; globalChunkIndex < streamedChunkOwnership.size(); globalChunkIndex++){
                if(streamedChunkOwnership[globalChunkIndex].first == -1){ // free chunk found
                    //DEBUG_PRINTF("found free chunk");
                    currentSampleStartIndex += globalChunkIndex * chunkLength;
                    chunkStartIndices[sampleIdentifier].push_back(currentSampleStartIndex);
                    streamedChunkOwnership[globalChunkIndex] = sampleIdentifier;
                    freeChunkFound = true;
                    break;
                }
            }
            //nothing free but maybe I can erase a sample that is not needed anymore
            if(!freeChunkFound){
                //no free chunk found, need to evict one
                for(int globalChunkIndex = 0; globalChunkIndex < streamedChunkOwnership.size(); globalChunkIndex++){
                    std::pair<int,int> ownersampleIdentifier = streamedChunkOwnership[globalChunkIndex];
                    bool sampleIdentifierInActiveRequests = false;
                    //maybe make this stdlib compatible.
                    for (auto it = activeRequests.begin(); it != activeRequests.end(); ++it) {
                        size_t requestId = it->first;
                        if(activeRequests[requestId].sampleIdentifier == ownersampleIdentifier){
                            sampleIdentifierInActiveRequests = true;
                            break;
                        }
                    }
                    if(!sampleIdentifierInActiveRequests){
                        this->eraseSample(ownersampleIdentifier);
                        currentSampleStartIndex += globalChunkIndex * chunkLength;
                        chunkStartIndices[sampleIdentifier].push_back(currentSampleStartIndex);
                        streamedChunkOwnership[globalChunkIndex] = sampleIdentifier;
                        freeChunkFound = true;
                        break;
                    }
                }
            }
            //buffer is full - therefore too small.
            if(!freeChunkFound){
                DEBUG_PRINTF("StreamingBuffer::streamSamples: no free chunk found to load sample for sampleId1 %d sampleId2 %d. buffer too small.\n", sampleIdentifier.first, sampleIdentifier.second);
                throw std::runtime_error("StreamingBuffer::streamSamples: no free chunk found. buffer too small.");
            }

            //--------------------DO THE ACTUAL STREAM OF THE CHUNK----------------------
            //TODO: make stereo to mono as above.
            int channel = 0;
            //DEBUG_PRINTF("loading chunk from sdcardindex %d\n", sdFileReadEndIndex);
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
    if(resourceManager->getStreamStartsFlag()){
        this->streamStarts();
    } else {
        this->streamChunks();
    }
    resourceManager->setStreamLoadingFlag(false);
    DEBUG_PRINTF("stream on thread complete ----------------------\n");
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
    bool printFlag = resourceManager->getStreamLoadingFlag();
    printf("  streamLoadingFlag: %s\n", printFlag ? "true" : "false");
    printFlag = resourceManager->getStreamStartsFlag();
    printf("  streamStartsFlag: %s\n", printFlag ? "true" : "false");
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
