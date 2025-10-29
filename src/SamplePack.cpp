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




//Example: bigSamplePackBufferLength = 44100 * 6 * 100;  6 seconds approx 1 MB we reserve 100 MB so 600 seconds. may be excessive.
SamplePack::SamplePack(ResourceManager* resourceManager,
    std::string samplePackName, std::string samplePackFolderName, size_t bufferSize):
    resourceManager(resourceManager),
    streamingBuffer(resourceManager, bufferSize, samplePackName, samplePackFolderName){
}
