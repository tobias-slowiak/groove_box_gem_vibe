#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>
//compile
#include <vector>
#include <string>
#include <map>
#include <utility>
#include <algorithm>
#include <cmath>
#include <dirent.h>
#include "../include/DebugLog.h"

#include "../include/ResourceManager.h"
#include "../include/StreamingBuffer.h"
#include "../include/SamplePack.h"
#include "../include/Voices.h"
#include <cassert>


SamplePack::SamplePack(ResourceManager* resourceManager,
            Voices* voices,
            std::string samplePackName, std::string samplePackFolderPath, size_t bufferSizeInFrames)
            :voices(voices),
            samplePackName(samplePackName),
            samplePackFolderPath(samplePackFolderPath),
            availableSamples(),
            availableKeys(),
            streamingBuffer(resourceManager,
                bufferSizeInFrames,
                voices->maxVoices,
                samplePackName + "_buffer", samplePackFolderPath,
                availableSamples){
    DEBUG_PRINTF("SamplePack ctor: name=%s folder=%s bufferFrames=%u\n",
        samplePackName.c_str(), samplePackFolderPath.c_str(), (unsigned int)bufferSizeInFrames);
    getAvailableSamples();
    streamingBuffer.initializeSamples(availableSamples);
    streamingBuffer.mutate();
    DEBUG_PRINTF("SamplePack ctor: found %zu samples across %zu keys\n",
        availableSamples.size(), availableKeys.size());
    streamingBuffer.streamStarts();
    streamingBuffer.stream();
    StreamingMessage msg;
    streamingBuffer.processBlockwise();
}

std::unordered_map<SampleIdentifier, size_t>& SamplePack::getAvailableSamples(){
    //TODO: maybe better do mutation on seperate thread.
    DEBUG_PRINTF("SamplePack::getAvailableSamples scanning folder %s\n", samplePackFolderPath.c_str());
    availableSamples.clear();
    availableKeys.clear();
    std::vector<std::string> wavFiles = listWavFiles();
    DEBUG_PRINTF("SamplePack::getAvailableSamples: %zu wav files listed\n", wavFiles.size());
    for(int key = 0; key < 128; key++){
        for(int velocity = 0; velocity < 128; velocity++){
            SampleIdentifier sampleIdentifier = {key, velocity};
            std::string filename = samplePackFolderPath + "/" + std::to_string(key) + "_" + std::to_string(velocity) + ".wav";
            if(std::find(wavFiles.begin(), wavFiles.end(), filename) != wavFiles.end()){
                int frames = AudioFileUtilities::getNumFrames(filename);
                if(frames > 0){
                    //DEBUG_PRINTF("initializing %d frames for sample %d %d\n", frames, sampleIdentifier.first, sampleIdentifier.second);
                    availableSamples[sampleIdentifier] = static_cast<size_t>(frames);
                    availableKeys.insert(key);
                } else {
                    rt_printf("SamplePack: unable to read frames for %s (error %d), skipping\n", filename.c_str(), frames);
                }
            }
        }
    }
    DEBUG_PRINTF("SamplePack::getAvailableSamples: usable samples=%zu keys=%zu\n",
        availableSamples.size(), availableKeys.size());
    return availableSamples;
}

void SamplePack::initializeBuffer(){
    DEBUG_PRINTF("SamplePack::initializeBuffer\n");
    getAvailableSamples();
    streamingBuffer.initializeSamples(availableSamples);
    streamingBuffer.streamStarts();
}


std::vector<std::string> SamplePack::listWavFiles() {
    //printf("Listing WAV files in folder: %s\n", samplePackFolderPath.c_str());
    std::vector<std::string> result;
    DIR* dir = opendir(samplePackFolderPath.c_str());
    if (dir == nullptr) return result;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".wav") {
            result.push_back(samplePackFolderPath + "/" + name);
        }
    }
    //printf("Found WAV files: %d\n", result.size());

    closedir(dir);
    return result;
}

SampleIdentifier SamplePack::findClosestSample(SampleIdentifier sampleIdentifier){
    int largerKey, smallerKey, bestKey;
    largerKey = smallerKey = bestKey = sampleIdentifier.first;
    //find closest key available
    while(true){
        auto keyIt = availableKeys.find(smallerKey);
        if(keyIt != availableKeys.end()){
            bestKey = smallerKey;
            break;
        }
        keyIt = availableKeys.find(largerKey);
        if(keyIt != availableKeys.end()){
            bestKey = largerKey;
            break;
        }
        smallerKey--;
        assert(smallerKey >= 0 && "SamplePack::findClosestSample went out of bounds low key");
        largerKey++;
        assert(largerKey <= 256 && "SamplePack::findClosestSample went out of bounds high key");
    }
    int smallerVelocity, largerVelocity, bestVelocity;
    smallerVelocity = largerVelocity = bestVelocity = sampleIdentifier.second;
    while(true){
        auto it = availableSamples.find({bestKey, smallerVelocity});
        if (it != availableSamples.end()) {
            bestVelocity = smallerVelocity;
            break;
        }
        it = availableSamples.find({bestKey, largerVelocity});
        if (it != availableSamples.end()) {
            bestVelocity = largerVelocity;
            break;
        }
        smallerVelocity--;
        assert(smallerVelocity > 0 && "SamplePack::findClosestSample went out of bounds low vel");
        largerVelocity++;
        assert(largerVelocity <= 256 && "SamplePack::findClosestSample went out of bounds high vel");
    }
    return {bestKey, bestVelocity};
}

void SamplePack::triggerVoice(SampleIdentifier sampleIdentifier){
    assert(sampleIdentifier.second > 0 && "SamplePack::triggerVoice triggered with vel==0");
    float gain = 1.0f / sampleIdentifier.second; //TODO: make logarithmic?
    SampleIdentifier closestSample = findClosestSample(sampleIdentifier);
    StreamingBufferIterator& iterator = streamingBuffer.begin(closestSample, SBIType::Read);
    float playbackRate = powf(2.0f, (float)(sampleIdentifier.first - closestSample.first) / 12.0f);
    voices->triggerVoice(iterator, playbackRate, gain);
}
