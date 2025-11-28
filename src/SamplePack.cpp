#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>
//test
#include <vector>
#include <string>
#include <map>
#include <utility>
#include <algorithm>

#include "../include/ResourceManager.h"
#include "../include/StreamingBuffer.h"
#include "../include/SamplePack.h"
#include <cassert>




//Example: bigSamplePackBufferLength = 44100 * 6 * 100;  6 seconds approx 1 MB we reserve 100 MB so 600 seconds. may be excessive.
SamplePack::SamplePack(ResourceManager* resourceManager,
            std::string samplePackName, std::string samplePackFolderPath, size_t bufferSize)
            :resourceManager(resourceManager),
            samplePackName(samplePackName),
            samplePackFolderPath(samplePackFolderPath){
    sampleFileNames = getAvailableSamples(samplePackFolderName, availableSamples);
    
    streamingBuffer = StreamingBuffer(resourceManager,
            int totalNumberOfChunks, int chunkLength,
            int totalNumberOfIterators,
            samplePackName + "_buffer", folderPath,
            std::unordered_map<SampleIdentifier, size_t>& availableSamples)
    
}

void SamplePack::getAvailableSamples(std::string samplePackFolderName, std::unordered_map<SampleIdentifier, size_t>& availableSamples){
    //TODO: maybe better do mutation on seperate thread.
    availableSamples.clear();
    std::vector<std::string> wavFiles = listWavFiles(samplePackFolderName);
    for(int key = 0; key < 128; key++){
        for(int velocity = 0; velocity < 128; velocity++){
            SampleIdentifier sampleIdentifier = {key, velocity};
            std::string filename = samplePackFolderPath + "/" + std::to_string(key) + "_" + std::to_string(velocity) + ".wav";
            if(std::find(wavFiles.begin(), wavFiles.end(), filename) != wavFiles.end()){
                availableSamples[sampleIdentifier] = AudioFileUtilities::getNumFrames(filename);
            }
        }
    }
}


std::vector<std::string> SamplePack::listWavFiles(const std::string folderPath) {
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