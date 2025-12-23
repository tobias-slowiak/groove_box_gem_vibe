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
#include "../../include/general/DebugLog.h"

#include "../../include/general/ResourceManager.h"
#include "../../include/streamingBuffer/StreamingBuffer.h"
#include "../../include/audio/SamplePack.h"
#include "../../include/audio/Voices.h"
#include <cassert>


SamplePack::SamplePack(ResourceManager& resourceManager,
            Voices* voices,
            std::string samplePackName, std::string samplePackFolderPath, size_t bufferSizeInFrames)
            :voices([&](){ assert(voices != nullptr && "SamplePack ctor voices null"); return voices; }()),
            samplePackName(std::move(samplePackName)),
            samplePackFolderPath(std::move(samplePackFolderPath)),
            availableSamples(),
            availableKeys(),
            streamingBuffer(resourceManager,
                bufferSizeInFrames,
                [&](){ assert(voices != nullptr && "SamplePack ctor voices null while reading maxVoices"); assert(voices->maxVoices > 0 && "SamplePack ctor maxVoices must be positive"); return voices->maxVoices; }(),
                this->samplePackName + "_buffer", this->samplePackFolderPath,
                availableSamples),
            initTaskName(this->samplePackName + "_ITask"),
            initTask(this, initTaskPrio, initTaskName)
{
    assert(!this->samplePackName.empty() && "SamplePack ctor samplePackName empty");
    assert(!this->samplePackFolderPath.empty() && "SamplePack ctor samplePackFolderPath empty");
    assert(bufferSizeInFrames > 0 && "SamplePack ctor bufferSizeInFrames must be > 0");
    DEBUG_PRINTF("SamplePack ctor: name=%s folder=%s bufferFrames=%u\n",
        this->samplePackName.c_str(), this->samplePackFolderPath.c_str(), (unsigned int)bufferSizeInFrames);

    initWork();

}

void SamplePack::taskWorkMessage(std::string& taskName, DefaultTaskMessage msg){
	if(taskName == samplePackName + "_ITask"){
		initWork();
		return;
	}
	throw std::runtime_error("SamplePack::taskWorkMessage invoked with task name " + taskName);
}

//TODO: problem: when i do this 2 times for the same folder there is a problem. do voicestest twice to trigger the error.
void SamplePack::initForFolder(std::string samplePackFolderPath){
    assert(!samplePackFolderPath.empty() && "SamplePack::initForFolder empty folder path");
    this->samplePackFolderPath = samplePackFolderPath;
    streamingBuffer.setFolderPath(samplePackFolderPath);
    DefaultTaskMessage msg;
    initTask.pushMessage(TaskMessageTarget::TaskThread, msg);
    initTask.taskCheckAndWorkMessages();
    //todo make some waiting functionality here that does nothing while on sleep
}

void SamplePack::initWork(){
    streamingBuffer.clear();
    getAvailableSamples();
    DEBUG_PRINTF("SamplePack ctor: found %zu samples across %zu keys\n",
        availableSamples.size(), availableKeys.size());
    streamingBuffer.initializeForNewSamplePack(availableSamples);
}

std::unordered_map<SampleIdentifier, size_t>& SamplePack::getAvailableSamples(){
    //TODO: maybe better do mutation on seperate thread.
    assert(!samplePackFolderPath.empty() && "SamplePack::getAvailableSamples empty folder path");
    DEBUG_PRINTF("SamplePack::getAvailableSamples scanning folder %s\n", samplePackFolderPath.c_str());
    availableSamples.clear();
    availableKeys.clear();
    std::vector<std::string> wavFiles = listWavFiles();
    DEBUG_PRINTF("SamplePack::getAvailableSamples: %zu wav files listed\n", wavFiles.size());
    maxAvailableVelocity = 0;
    for(int key = 0; key < 128; key++){
        for(int velocity = 0; velocity < 128; velocity++){
            SampleIdentifier sampleIdentifier = {key, velocity};
            std::string filename = samplePackFolderPath + "/" + std::to_string(key) + "_" + std::to_string(velocity) + ".wav";
            if(std::find(wavFiles.begin(), wavFiles.end(), filename) != wavFiles.end()){
                if(velocity > maxAvailableVelocity) maxAvailableVelocity = velocity;
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
    if(availableSamples.size() != wavFiles.size()) throw std::runtime_error("SamplePack: there are files that are not in the right naming convention note_velocity.wav!!");
    DEBUG_PRINTF("SamplePack::getAvailableSamples: usable samples=%zu keys=%zu\n with max Velocity: %d",
        availableSamples.size(), availableKeys.size(), maxAvailableVelocity);
    return availableSamples;
}


std::vector<std::string> SamplePack::listWavFiles() {
    assert(!samplePackFolderPath.empty() && "SamplePack::listWavFiles empty folder path");
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
    assert(!availableSamples.empty() && "SamplePack::findClosestSample no available samples");
    assert(!availableKeys.empty() && "SamplePack::findClosestSample no available keys");
    assert(sampleIdentifier.first >= 0 && sampleIdentifier.first < 128 && "SamplePack::findClosestSample key out of MIDI range");
    assert(sampleIdentifier.second >= 0 && sampleIdentifier.second < 128 && "SamplePack::findClosestSample velocity out of MIDI range");
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

void SamplePack::triggerVoice(int note, int midiVelocity){
    assert(voices != nullptr && "SamplePack::triggerVoice voices null");
    assert(!availableSamples.empty() && "SamplePack::triggerVoice no available samples");
    assert(note >= 0 && note < 128 && "SamplePack::triggerVoice key out of MIDI range");
    assert(midiVelocity >= 0 && midiVelocity < 128 && "SamplePack::triggerVoice velocity out of MIDI range");
    assert(midiVelocity > 0 && "SamplePack::triggerVoice triggered with vel==0");
    SampleIdentifier sampleIdentifier{note, midiToSampleVelocity(midiVelocity)};
    SampleIdentifier closestSample = findClosestSample(sampleIdentifier);
    assert(availableSamples.find(closestSample) != availableSamples.end() && "SamplePack::triggerVoice closestSample not in availableSamples");
    float playbackRate = powf(2.0f, (float)(sampleIdentifier.first - closestSample.first) / 12.0f);
    StreamingBufferIterator& iterator = streamingBuffer.begin(closestSample, SBIType::Read, playbackRate);
    DEBUG_RT_PRINTF("playbackrate %f on original sample %d, %d with chosen sample %d %d\n", playbackRate, sampleIdentifier.first, sampleIdentifier.second, closestSample.first, closestSample.second);
    voices->triggerVoice(iterator, note, playbackRate);
}

void SamplePack::triggerOff(int note){
    voices->triggerOff(note);
}

int SamplePack::midiToSampleVelocity(int midiVelocity){
    return std::ceil(((float)midiVelocity / (float)127) * maxAvailableVelocity);
}

void SamplePack::processBlockwise(){
    streamingBuffer.processBlockwise();
}
