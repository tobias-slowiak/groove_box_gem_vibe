#include <algorithm>
#include <cerrno>
#include <cmath>
#include <stdexcept>
#include <sys/stat.h>
#include <utility>

#include "../../include/audio/Samplers.h"
#include "../../include/general/BasicUtilities.h"
#include "../../include/general/ResourceManager.h"

namespace {
constexpr int DEFAULT_MAX_SAMPLE_SECONDS = 60;
constexpr int DEFAULT_MAX_CONCURRENT_SAMPLES = 5;
constexpr int DEFAULT_MAX_SAMPLE_SLOTS = 64;
constexpr int SAMPLER_MAX_ACTIVE_VOICES = 24;
constexpr int SAMPLER_TOTAL_ITERATORS = SAMPLER_MAX_ACTIVE_VOICES + 8;
constexpr const char* SAMPLER_STORAGE_FOLDER = "/root/Bela/Samples/Samplers";
constexpr float SAMPLER_VOICE_ATTACK_SECONDS = 0.01f;
constexpr float SAMPLER_VOICE_DECAY_SECONDS = 0.0f;
constexpr float SAMPLER_VOICE_SUSTAIN_LEVEL = 1.0f;
constexpr float SAMPLER_VOICE_RELEASE_SECONDS = 0.1f;

float clampPitchShift(float value)
{
    return std::max(-36.0f, std::min(36.0f, value));
}
}

Samplers::Samplers(ResourceManager& resourceManager)
    : resourceManager(resourceManager),
      maxSampleLengthInFrames(static_cast<size_t>(resourceManager.audioFramesPerSecond) * DEFAULT_MAX_SAMPLE_SECONDS),
      maxSampleSlots(DEFAULT_MAX_SAMPLE_SLOTS),
      availableSamples([this] {
          std::unordered_map<SampleIdentifier, size_t> map;
          map.reserve(static_cast<size_t>(maxSampleSlots));
          for(int slot = 0; slot < maxSampleSlots; ++slot){
              map.emplace(SampleIdentifier{slot, 0}, maxSampleLengthInFrames);
          }
          return map;
      }()),
      streamingBuffer(resourceManager,
                      maxSampleLengthInFrames * DEFAULT_MAX_CONCURRENT_SAMPLES,
                      SAMPLER_TOTAL_ITERATORS,
                      "Sampler_buffer",
                      SAMPLER_STORAGE_FOLDER,
                      availableSamples)
{
    samplerEntries.reserve(static_cast<size_t>(maxSampleSlots));
    activeVoices.reserve(SAMPLER_MAX_ACTIVE_VOICES);
    if(::mkdir(SAMPLER_STORAGE_FOLDER, 0777) != 0 && errno != EEXIST){
        rt_printf("Samplers: unable to create folder %s\n", SAMPLER_STORAGE_FOLDER);
    }
    streamingBuffer.initializeForLoopers(availableSamples);
}

void Samplers::processBlockwise()
{
    streamingBuffer.processBlockwise();
}

float Samplers::process(float inFrame)
{
    if(recordIteratorPtr){
        *(*recordIteratorPtr) = inFrame;
        ++(*recordIteratorPtr);
        ++recordingFramesWritten;
        if(recordingFramesWritten >= maxSampleLengthInFrames){
            finalizeRecording(true);
        }
    }
    return processVoices();
}

bool Samplers::toggleRecording(int samplerIndex)
{
    if(isRecording()){
        if(recordingSamplerIndex == samplerIndex){
            stopRecording();
        }
        return false;
    }
    return startRecording(samplerIndex);
}

bool Samplers::startRecording(int samplerIndex)
{
    if(isRecording()){
        return false;
    }

    const int ensuredIndex = ensureSamplerExists(samplerIndex);
    if(ensuredIndex < 0){
        return false;
    }

    SamplerEntry& sampler = VEC_AT(samplerEntries, ensuredIndex);

    for(auto it = activeVoices.begin(); it != activeVoices.end(); ){
        bool sameSample = it->iteratorPtr &&
            it->iteratorPtr->sampleIdentifier == sampler.sampleIdentifier;
        if(sameSample){
            it->iteratorPtr->release();
            it = activeVoices.erase(it);
        } else {
            ++it;
        }
    }

    availableSamples[sampler.sampleIdentifier] = maxSampleLengthInFrames;
    recordIteratorPtr = &streamingBuffer.begin(sampler.sampleIdentifier, SBIType::Write);

    sampler.hasAudio = false;
    sampler.sampleLengthInFrames = 0;
    sampler.slices.clear();
    sampler.waveformPreviewDirty = true;
    sampler.waveformPreview.clear();

    recordingSamplerIndex = ensuredIndex;
    recordingFramesWritten = 0;
    return true;
}

bool Samplers::stopRecording()
{
    if(!isRecording()){
        return false;
    }
    finalizeRecording(true);
    return true;
}

bool Samplers::isRecording() const
{
    return recordIteratorPtr != nullptr;
}

bool Samplers::isRecording(int samplerIndex) const
{
    return isRecording() && recordingSamplerIndex == samplerIndex;
}

int Samplers::getRecordingSamplerIndex() const
{
    return recordingSamplerIndex;
}

int Samplers::getNumSamples() const
{
    return static_cast<int>(samplerEntries.size());
}

int Samplers::getMaxSampleSlots() const
{
    return maxSampleSlots;
}

int Samplers::getSelectableSampleCount() const
{
    const int withNewSlot = getNumSamples() + 1;
    return std::min(maxSampleSlots, withNewSlot);
}

int Samplers::clampSamplerSelection(int samplerIndex) const
{
    const int selectableCount = getSelectableSampleCount();
    if(selectableCount <= 0){
        return 0;
    }
    int wrapped = samplerIndex % selectableCount;
    if(wrapped < 0){
        wrapped += selectableCount;
    }
    return wrapped;
}

bool Samplers::hasSampleAudio(int samplerIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    return sampler && sampler->hasAudio;
}

size_t Samplers::getSampleLength(int samplerIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler){
        return 0;
    }
    return sampler->sampleLengthInFrames;
}

int Samplers::getNumSlices(int samplerIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler){
        return 0;
    }
    return static_cast<int>(sampler->slices.size());
}

int Samplers::clampSliceSelection(int samplerIndex, int sliceIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || sampler->slices.empty()){
        return 0;
    }
    const int sliceCount = static_cast<int>(sampler->slices.size());
    int wrapped = sliceIndex % sliceCount;
    if(wrapped < 0){
        wrapped += sliceCount;
    }
    return wrapped;
}

bool Samplers::autoSlice(int samplerIndex, int numberOfSlices)
{
    SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || !sampler->hasAudio || sampler->sampleLengthInFrames == 0){
        return false;
    }

    numberOfSlices = std::max(1, std::min(numberOfSlices, 32));
    sampler->slices.clear();
    sampler->slices.reserve(static_cast<size_t>(numberOfSlices));

    for(int i = 0; i < numberOfSlices; ++i){
        size_t start = (sampler->sampleLengthInFrames * static_cast<size_t>(i)) / static_cast<size_t>(numberOfSlices);
        size_t end = (sampler->sampleLengthInFrames * static_cast<size_t>(i + 1)) / static_cast<size_t>(numberOfSlices);
        if(end <= start){
            end = std::min(sampler->sampleLengthInFrames, start + 1);
        }
        sampler->slices.push_back({start, end, 0.0f});
    }

    return true;
}

bool Samplers::moveSliceBoundary(int samplerIndex,
                                 int sliceIndex,
                                 ManualSliceBoundary boundary,
                                 int frameDelta)
{
    SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || !sampler->hasAudio || sampler->slices.empty()){
        return false;
    }

    sliceIndex = clampSliceSelection(samplerIndex, sliceIndex);
    SamplerSliceMetadata& slice = VEC_AT(sampler->slices, sliceIndex);

    if(boundary == ManualSliceBoundary::Start){
        long candidate = static_cast<long>(slice.startFrame) + static_cast<long>(frameDelta);
        long maxStart = static_cast<long>(slice.endFrame) - 1;
        if(maxStart < 0){
            maxStart = 0;
        }
        candidate = std::max<long>(0, std::min(candidate, maxStart));
        slice.startFrame = static_cast<size_t>(candidate);
        return true;
    }

    long candidate = static_cast<long>(slice.endFrame) + static_cast<long>(frameDelta);
    long minEnd = static_cast<long>(slice.startFrame) + 1;
    long maxEnd = static_cast<long>(sampler->sampleLengthInFrames);
    candidate = std::max(minEnd, std::min(candidate, maxEnd));
    slice.endFrame = static_cast<size_t>(candidate);
    return true;
}

float Samplers::getSliceStartNormalized(int samplerIndex, int sliceIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || !sampler->hasAudio || sampler->sampleLengthInFrames == 0 || sampler->slices.empty()){
        return 0.0f;
    }
    sliceIndex = clampSliceSelection(samplerIndex, sliceIndex);
    const SamplerSliceMetadata& slice = VEC_AT(sampler->slices, sliceIndex);
    return static_cast<float>(slice.startFrame) / static_cast<float>(sampler->sampleLengthInFrames);
}

float Samplers::getSliceEndNormalized(int samplerIndex, int sliceIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || !sampler->hasAudio || sampler->sampleLengthInFrames == 0 || sampler->slices.empty()){
        return 1.0f;
    }
    sliceIndex = clampSliceSelection(samplerIndex, sliceIndex);
    const SamplerSliceMetadata& slice = VEC_AT(sampler->slices, sliceIndex);
    return static_cast<float>(slice.endFrame) / static_cast<float>(sampler->sampleLengthInFrames);
}

float Samplers::getSlicePitchSemitones(int samplerIndex, int sliceIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || sampler->slices.empty()){
        return 0.0f;
    }
    sliceIndex = clampSliceSelection(samplerIndex, sliceIndex);
    return VEC_AT(sampler->slices, sliceIndex).pitchShiftSemitones;
}

bool Samplers::adjustSlicePitchSemitones(int samplerIndex, int sliceIndex, float deltaSemitones)
{
    SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || sampler->slices.empty()){
        return false;
    }
    sliceIndex = clampSliceSelection(samplerIndex, sliceIndex);
    SamplerSliceMetadata& slice = VEC_AT(sampler->slices, sliceIndex);
    slice.pitchShiftSemitones = clampPitchShift(slice.pitchShiftSemitones + deltaSemitones);
    return true;
}

int Samplers::getManualEditStepFrames(int samplerIndex) const
{
    const SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || sampler->sampleLengthInFrames == 0){
        return 1;
    }
    const size_t step = std::max<size_t>(1, sampler->sampleLengthInFrames / 128);
    return static_cast<int>(step);
}

SamplerKeyboardMode Samplers::getKeyboardMode() const
{
    return keyboardMode;
}

void Samplers::cycleKeyboardMode(int direction)
{
    const int modeCount = 3;
    int shift = 0;
    if(direction > 0){
        shift = 1;
    } else if(direction < 0){
        shift = -1;
    }
    int modeIndex = static_cast<int>(keyboardMode) + shift;
    modeIndex %= modeCount;
    if(modeIndex < 0){
        modeIndex += modeCount;
    }
    keyboardMode = static_cast<SamplerKeyboardMode>(modeIndex);
}

std::string Samplers::getKeyboardModeName() const
{
    switch(keyboardMode){
        case SamplerKeyboardMode::Off:
            return "Off";
        case SamplerKeyboardMode::Slices:
            return "Slices";
        case SamplerKeyboardMode::PitchFromSelectedSlice:
            return "Pitch";
    }
    return "Off";
}

bool Samplers::isKeyboardPlaybackEnabled() const
{
    return keyboardMode != SamplerKeyboardMode::Off;
}

bool Samplers::triggerNoteOn(int note,
                             int velocity,
                             int samplerIndex,
                             int selectedSliceIndex)
{
    if(!isKeyboardPlaybackEnabled()){
        return false;
    }
    if(note < 0 || note >= 128 || velocity <= 0){
        return false;
    }

    SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler || !sampler->hasAudio || sampler->slices.empty()){
        return false;
    }

    int sliceIndex = 0;
    float semitoneShift = 0.0f;
    if(!resolveSliceForNote(*sampler, note, selectedSliceIndex, sliceIndex, semitoneShift)){
        return false;
    }

    const SamplerSliceMetadata& slice = VEC_AT(sampler->slices, sliceIndex);
    if(slice.endFrame <= slice.startFrame){
        return false;
    }

    const float playbackRate = std::max(0.05f, std::pow(2.0f, semitoneShift / 12.0f));
    const float gain = clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);

    if(static_cast<int>(activeVoices.size()) >= SAMPLER_MAX_ACTIVE_VOICES){
        SamplerVoice& oldest = VEC_AT(activeVoices, 0);
        if(oldest.iteratorPtr && oldest.iteratorPtr->sampleIdentifier.first != ITERATOR_INVALID){
            oldest.iteratorPtr->release();
        }
        activeVoices.erase(activeVoices.begin());
    }

    StreamingBufferIterator& iterator = streamingBuffer.begin(
        sampler->sampleIdentifier,
        SBIType::Read,
        std::max(1.0f, playbackRate));

    if(!iterator.seek(slice.startFrame)){
        iterator.release();
        return false;
    }

    SamplerVoice voice(resourceManager,
                       SAMPLER_VOICE_ATTACK_SECONDS,
                       SAMPLER_VOICE_DECAY_SECONDS,
                       SAMPLER_VOICE_SUSTAIN_LEVEL,
                       SAMPLER_VOICE_RELEASE_SECONDS);
    voice.iteratorPtr = &iterator;
    voice.note = note;
    voice.gain = gain;
    voice.playbackRate = static_cast<double>(playbackRate);
    voice.position = 0.0;
    voice.currentIndex = 0;
    voice.sliceLengthInFrames = slice.endFrame - slice.startFrame;
    voice.currentFrame = *iterator;

    if(voice.currentFrame == END_OF_SAMPLE){
        iterator.release();
        return false;
    }

    activeVoices.push_back(std::move(voice));
    return true;
}

void Samplers::triggerNoteOff(int note)
{
    for(auto it = activeVoices.begin(); it != activeVoices.end(); ++it){
        if(it->note == note){
            if(!it->noteReleased){
                it->noteReleased = true;
                it->adsr.noteOff();
            }
        }
    }
}

const std::vector<float>& Samplers::getWaveformPreview(int samplerIndex, size_t points)
{
    static const std::vector<float> emptyPreview;
    SamplerEntry* sampler = tryGetSampler(samplerIndex);
    if(!sampler){
        return emptyPreview;
    }

    if(points < 8){
        points = 8;
    }

    if(sampler->waveformPreviewDirty || sampler->waveformPreview.size() != points){
        rebuildWaveformPreview(*sampler, points);
    }

    return sampler->waveformPreview;
}

int Samplers::ensureSamplerExists(int samplerIndex)
{
    if(samplerIndex < 0){
        return -1;
    }

    if(samplerIndex < static_cast<int>(samplerEntries.size())){
        return samplerIndex;
    }

    if(samplerIndex != static_cast<int>(samplerEntries.size())){
        return -1;
    }

    if(static_cast<int>(samplerEntries.size()) >= maxSampleSlots){
        return -1;
    }

    SamplerEntry entry;
    entry.sampleIdentifier = {samplerIndex, 0};
    samplerEntries.push_back(entry);
    return samplerIndex;
}

Samplers::SamplerEntry* Samplers::tryGetSampler(int samplerIndex)
{
    if(samplerIndex < 0 || samplerIndex >= static_cast<int>(samplerEntries.size())){
        return nullptr;
    }
    return &VEC_AT(samplerEntries, samplerIndex);
}

const Samplers::SamplerEntry* Samplers::tryGetSampler(int samplerIndex) const
{
    if(samplerIndex < 0 || samplerIndex >= static_cast<int>(samplerEntries.size())){
        return nullptr;
    }
    return &VEC_AT(samplerEntries, samplerIndex);
}

void Samplers::initializeDefaultSlice(SamplerEntry& sampler)
{
    sampler.slices.clear();
    if(!sampler.hasAudio || sampler.sampleLengthInFrames == 0){
        return;
    }
    sampler.slices.push_back({0, sampler.sampleLengthInFrames, 0.0f});
}

void Samplers::rebuildWaveformPreview(SamplerEntry& sampler, size_t points)
{
    sampler.waveformPreview.assign(points, 0.0f);
    sampler.waveformPreviewDirty = false;

    if(!sampler.hasAudio || sampler.sampleLengthInFrames == 0){
        return;
    }

    try {
        StreamingBufferIterator& iterator = streamingBuffer.begin(sampler.sampleIdentifier, SBIType::Read, 1.0f);
        for(size_t i = 0; i < points; ++i){
            size_t sourceIndex = 0;
            if(points > 1){
                sourceIndex = ((sampler.sampleLengthInFrames - 1) * i) / (points - 1);
            }
            if(!iterator.seek(sourceIndex)){
                sampler.waveformPreview[i] = 0.0f;
                continue;
            }
            float frame = *iterator;
            if(frame == END_OF_SAMPLE){
                frame = 0.0f;
            }
            sampler.waveformPreview[i] = clamp(frame, -1.0f, 1.0f);
        }
        if(iterator.sampleIdentifier.first != ITERATOR_INVALID){
            iterator.release();
        }
    } catch(const std::exception& e){
        rt_printf("Samplers::rebuildWaveformPreview warning: %s\n", e.what());
        std::fill(sampler.waveformPreview.begin(), sampler.waveformPreview.end(), 0.0f);
    }
}

void Samplers::finalizeRecording(bool forceStop)
{
    if(!recordIteratorPtr || recordingSamplerIndex < 0){
        return;
    }

    SamplerEntry* sampler = tryGetSampler(recordingSamplerIndex);
    if(!sampler){
        recordIteratorPtr->release();
        recordIteratorPtr = nullptr;
        recordingSamplerIndex = -1;
        recordingFramesWritten = 0;
        return;
    }

    size_t finalFrames = std::min(recordingFramesWritten, maxSampleLengthInFrames);
    if(finalFrames == 0){
        *(*recordIteratorPtr) = 0.0f;
        ++(*recordIteratorPtr);
        finalFrames = 1;
    }

    sampler->sampleLengthInFrames = finalFrames;
    sampler->hasAudio = true;
    sampler->waveformPreviewDirty = true;

    availableSamples[sampler->sampleIdentifier] = finalFrames;
    *(*recordIteratorPtr) = END_OF_SAMPLE;
    recordIteratorPtr->flush();
    recordIteratorPtr->release();

    recordIteratorPtr = nullptr;
    recordingSamplerIndex = -1;
    recordingFramesWritten = 0;

    initializeDefaultSlice(*sampler);

    if(forceStop){
        rebuildWaveformPreview(*sampler, 128);
    }
}

float Samplers::processVoices()
{
    float mix = 0.0f;
    for(auto it = activeVoices.begin(); it != activeVoices.end(); ){
        if(!it->adsr.isOn()){
            if(it->iteratorPtr && it->iteratorPtr->sampleIdentifier.first != ITERATOR_INVALID){
                it->iteratorPtr->release();
            }
            it = activeVoices.erase(it);
            continue;
        }

        if(!it->iteratorPtr || it->iteratorPtr->sampleIdentifier.first == ITERATOR_INVALID){
            it = activeVoices.erase(it);
            continue;
        }

        if(it->position >= static_cast<double>(it->sliceLengthInFrames)){
            it->iteratorPtr->release();
            it = activeVoices.erase(it);
            continue;
        }

        int targetIndex = static_cast<int>(it->position);
        if(targetIndex < it->currentIndex){
            targetIndex = it->currentIndex;
        }

        while(it->currentIndex < targetIndex){
            ++(*it->iteratorPtr);
            ++it->currentIndex;
            if(it->currentIndex >= static_cast<int>(it->sliceLengthInFrames)){
                break;
            }
            it->currentFrame = *(*it->iteratorPtr);
            if(it->currentFrame == END_OF_SAMPLE){
                break;
            }
        }

        if(it->currentIndex >= static_cast<int>(it->sliceLengthInFrames) || it->currentFrame == END_OF_SAMPLE){
            it->iteratorPtr->release();
            it = activeVoices.erase(it);
            continue;
        }

        mix += it->currentFrame * it->gain * it->adsr.process();
        it->position += it->playbackRate;
        ++it;
    }
    return mix;
}

bool Samplers::resolveSliceForNote(const SamplerEntry& sampler,
                                   int note,
                                   int selectedSliceIndex,
                                   int& outSliceIndex,
                                   float& outSemitoneShift) const
{
    if(sampler.slices.empty()){
        return false;
    }

    if(keyboardMode == SamplerKeyboardMode::Slices){
        const int sliceIndex = note - 60;
        if(sliceIndex < 0 || sliceIndex >= static_cast<int>(sampler.slices.size())){
            return false;
        }
        outSliceIndex = sliceIndex;
        outSemitoneShift = VEC_AT(sampler.slices, sliceIndex).pitchShiftSemitones;
        return true;
    }

    if(keyboardMode == SamplerKeyboardMode::PitchFromSelectedSlice){
        int sliceIndex = selectedSliceIndex;
        const int sliceCount = static_cast<int>(sampler.slices.size());
        if(sliceIndex < 0){
            sliceIndex = 0;
        }
        if(sliceIndex >= sliceCount){
            sliceIndex = sliceCount - 1;
        }
        outSliceIndex = sliceIndex;
        outSemitoneShift = VEC_AT(sampler.slices, sliceIndex).pitchShiftSemitones + static_cast<float>(note - 60);
        return true;
    }

    return false;
}
