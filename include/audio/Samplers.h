#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "../streamingBuffer/StreamingBuffer.h"
#include "ADSR.h"

class ResourceManager;

enum class SamplerKeyboardMode {
    Off,
    Slices,
    PitchFromSelectedSlice
};

enum class ManualSliceBoundary {
    Start,
    End
};

struct SamplerSliceMetadata {
    size_t startFrame = 0;        // inclusive
    size_t endFrame = 0;          // exclusive
    float pitchShiftSemitones = 0.0f;
};

class Samplers {
public:
    Samplers(ResourceManager& resourceManager);

    void processBlockwise();
    float process(float inFrame);

    bool toggleRecording(int samplerIndex);
    bool startRecording(int samplerIndex);
    bool stopRecording();

    bool isRecording() const;
    bool isRecording(int samplerIndex) const;
    int getRecordingSamplerIndex() const;

    int getNumSamples() const;
    int getMaxSampleSlots() const;
    int getSelectableSampleCount() const; // includes one additional "new" slot if possible
    int clampSamplerSelection(int samplerIndex) const;
    bool hasSampleAudio(int samplerIndex) const;
    size_t getSampleLength(int samplerIndex) const;

    int getNumSlices(int samplerIndex) const;
    int clampSliceSelection(int samplerIndex, int sliceIndex) const;
    bool autoSlice(int samplerIndex, int numberOfSlices);
    bool moveSliceBoundary(int samplerIndex,
                           int sliceIndex,
                           ManualSliceBoundary boundary,
                           int frameDelta);
    float getSliceStartNormalized(int samplerIndex, int sliceIndex) const;
    float getSliceEndNormalized(int samplerIndex, int sliceIndex) const;
    float getSlicePitchSemitones(int samplerIndex, int sliceIndex) const;
    bool adjustSlicePitchSemitones(int samplerIndex, int sliceIndex, float deltaSemitones);
    int getManualEditStepFrames(int samplerIndex) const;

    SamplerKeyboardMode getKeyboardMode() const;
    void cycleKeyboardMode(int direction);
    std::string getKeyboardModeName() const;
    bool isKeyboardPlaybackEnabled() const;

    bool triggerNoteOn(int note,
                       int velocity,
                       int samplerIndex,
                       int selectedSliceIndex);
    void triggerNoteOff(int note);

    const std::vector<float>& getWaveformPreview(int samplerIndex, size_t points = 128);

private:
    struct SamplerEntry {
        SampleIdentifier sampleIdentifier{0, 0};
        bool hasAudio = false;
        size_t sampleLengthInFrames = 0;
        std::vector<SamplerSliceMetadata> slices;
        std::vector<float> waveformPreview;
        bool waveformPreviewDirty = true;
    };

    struct SamplerVoice {
        SamplerVoice(ResourceManager& resourceManager,
                     float attack = 0.01f,
                     float decay = 0.0f,
                     float sustain = 1.0f,
                     float release = 0.1f)
            : adsr(attack, decay, sustain, release, resourceManager)
        {
            adsr.init();
        }

        SamplerVoice(SamplerVoice&&) noexcept = default;
        SamplerVoice& operator=(SamplerVoice&&) noexcept = default;
        SamplerVoice(const SamplerVoice&) = delete;
        SamplerVoice& operator=(const SamplerVoice&) = delete;

        StreamingBufferIterator* iteratorPtr = nullptr;
        int note = 0;
        float gain = 1.0f;
        double playbackRate = 1.0;
        double position = 0.0;
        int currentIndex = 0;
        size_t sliceLengthInFrames = 0;
        float currentFrame = 0.0f;
        bool noteReleased = false;
        ADSR adsr;
    };

    ResourceManager& resourceManager;

    size_t maxSampleLengthInFrames = 0;
    int maxSampleSlots = 0;

    std::unordered_map<SampleIdentifier, size_t> availableSamples;
    StreamingBuffer streamingBuffer;
    std::vector<SamplerEntry> samplerEntries;
    std::vector<SamplerVoice> activeVoices;

    int recordingSamplerIndex = -1;
    StreamingBufferIterator* recordIteratorPtr = nullptr;
    size_t recordingFramesWritten = 0;

    SamplerKeyboardMode keyboardMode = SamplerKeyboardMode::Off;

    int ensureSamplerExists(int samplerIndex);
    SamplerEntry* tryGetSampler(int samplerIndex);
    const SamplerEntry* tryGetSampler(int samplerIndex) const;

    void initializeDefaultSlice(SamplerEntry& sampler);
    void rebuildWaveformPreview(SamplerEntry& sampler, size_t points = 128);
    void finalizeRecording(bool forceStop);

    float processVoices();

    bool resolveSliceForNote(const SamplerEntry& sampler,
                             int note,
                             int selectedSliceIndex,
                             int& outSliceIndex,
                             float& outSemitoneShift) const;
};
