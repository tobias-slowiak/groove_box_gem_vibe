#pragma once

#include<Bela.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <atomic>
#include <cstdint>
#include "../streamingBuffer/StreamingBuffer.h"
#include "../general/TaskWrapper.h"
#include "Voices.h"
//compile
class StreamingBuffer;
using SampleIdentifier = std::pair<int, int>;
class ResourceManager;

struct SamplePackVoiceSettings {
    float attack = 0.0f;
    float decay = 0.0f;
    float sustain = 1.0f;
    float release = 0.1f;
    bool repeat = false;
};

class SamplePack {
public:
    // make one for an instrument samplepack (larger) and one for a drum sample pack (smaller)
    SamplePack(ResourceManager& resourceManager,
        std::string samplePackName, std::string samplePackFolderPath,
        size_t bufferSizeInFrames, bool autoInitialize = true);

    void initForFolder(std::string samplePackFolderPath);

    void taskWorkMessage(std::string& taskName, DefaultTaskMessage msg);

    void initWork();

    std::unordered_map<SampleIdentifier, size_t>& getAvailableSamples();

    std::vector<std::string> listWavFiles();

    //the two following methods are only for the bandwidth test
    bool streamerIsInFlight(){return streamingBuffer.streamerIsInFlight();}
    void streamFullSample(SampleIdentifier sampleIdentifier){streamingBuffer.streamFullSample(sampleIdentifier);}

    SampleIdentifier findClosestSample(SampleIdentifier sampleIdentifier);

    void triggerVoice(int note, int midiVelocity, bool gainFromVelocity = false, float explicitGain = 1.0f);

    void triggerOff(int note);

    int midiToSampleVelocity(int midiVelocity);

    void processBlockwise();

    float process();

    void printBufferInfo(){streamingBuffer.printInfo();}

    bool isLoading() const { return loading.load(std::memory_order_acquire); }
    bool hasLoadedSamples() const { return !availableSamples.empty(); }

    void setVoiceSettings(const SamplePackVoiceSettings& settings);
    SamplePackVoiceSettings getVoiceSettings() const { return voiceSettings; }

private:
    struct ZoneEntry {
        SampleIdentifier sampleIdentifier{0, 0};
        int lokey = 0;
        int hikey = 127;
        int lovel = 0;
        int hivel = 127;
        int pitchKeycenter = 60;
        float tuneCents = 0.0f;
        float volumeDb = 0.0f;
        float ampVeltrack = 100.0f;
        int offset = 0;
    };

    using ZoneIndexList = std::vector<uint16_t>;

    static constexpr int kMidiValueCount = 128;
    static constexpr int kLookupSize = kMidiValueCount * kMidiValueCount;
    static constexpr int kSampleIdentifierStride = 128;
    static constexpr const char* kReleaseTrigger = "release";

    static int clampMidiValue(int value);
    static bool parseInt(const std::string& text, int& out);
    static bool parseFloat(const std::string& text, float& out);
    static std::vector<std::string> splitTabRow(const std::string& line);
    static size_t lookupIndex(int note, int velocity);
    static float dbToLinear(float db);

    std::string resolveZoneTablePath(const std::string& instrumentId) const;
    void loadZonesFromFile(const std::string& zoneTablePath);
    bool loadLegacyNoteVelocityFolder();
    static bool parseLegacyFilename(const std::string& filename, int& key, int& velocity);
    void clearZoneState();
    const ZoneIndexList& resolveZoneIndices(int note, int velocity, bool release) const;
    void triggerZone(const ZoneEntry& zone, int note, int midiVelocity, bool gainFromVelocity, float explicitGain);
    SampleIdentifier makeSampleIdentifier(size_t sampleSlot) const;
    std::string resolveExistingPath(const std::string& preferred, const std::string& fallback) const;

    Voices voices;
    std::string samplePackName;
    std::string samplePackFolderPath;
    std::string vcslRootPath;
    std::string tableRootPath;
    std::unordered_map<SampleIdentifier, std::string> sampleFileMap;
    std::unordered_map<SampleIdentifier, size_t> availableSamples;
    std::unordered_map<std::string, SampleIdentifier> sampleIdentifierByRelPath;
    std::vector<ZoneEntry> attackZones;
    std::vector<ZoneEntry> releaseZones;
    std::vector<ZoneIndexList> attackLookup;
    std::vector<ZoneIndexList> releaseLookup;
    std::vector<uint32_t> attackRoundRobinCounter;
    std::vector<uint32_t> releaseRoundRobinCounter;
    std::vector<int> lastNoteVelocity;
    bool hasReleaseZones = false;
    StreamingBuffer streamingBuffer;

    int initTaskPrio = 70;
    std::string initTaskName;
    TaskWrapper<SamplePack, DefaultTaskMessage> initTask;
    std::atomic<bool> loading{false};
    int loadingIdleBlockCounter = 0;
    SamplePackVoiceSettings voiceSettings;
};
