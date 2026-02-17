#include "../../include/audio/Effects.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
static const float kPi = 3.14159265358979323846f;

float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(v, hi));
}

struct IEffectUnit {
    virtual ~IEffectUnit() {}
    virtual void setSampleRate(float sampleRate) = 0;
    virtual void setTempoBpm(float) {}
    virtual void setParameters(const EffectParameters& params) = 0;
    virtual float process(float input) = 0;
    virtual void reset() = 0;
};

class OnePoleLowPass final : public IEffectUnit {
public:
    explicit OnePoleLowPass(float sr) : sampleRate(sr) {}

    void setSampleRate(float sr) override
    {
        sampleRate = sr;
        recalc();
    }

    void setParameters(const EffectParameters& p) override
    {
        params = p;
        recalc();
    }

    float process(float input) override
    {
        if(!params.enabled) {
            return input;
        }
        z = z + alpha * (input - z);
        return input * (1.0f - mix) + z * mix;
    }

    void reset() override
    {
        z = 0.0f;
    }

private:
    void recalc()
    {
        const float fc = clampf(params.cutoffHz, 20.0f, sampleRate * 0.45f);
        const float rc = 1.0f / (2.0f * kPi * fc);
        const float dt = 1.0f / sampleRate;
        alpha = dt / (rc + dt);
        mix = clampf(params.mix, 0.0f, 1.0f);
    }

    float sampleRate;
    EffectParameters params;
    float alpha = 0.05f;
    float mix = 1.0f;
    float z = 0.0f;
};

class OnePoleHighPass final : public IEffectUnit {
public:
    explicit OnePoleHighPass(float sr) : sampleRate(sr) {}

    void setSampleRate(float sr) override
    {
        sampleRate = sr;
        recalc();
    }

    void setParameters(const EffectParameters& p) override
    {
        params = p;
        recalc();
    }

    float process(float input) override
    {
        if(!params.enabled) {
            return input;
        }
        const float y = alpha * (prevY + input - prevX);
        prevX = input;
        prevY = y;
        return input * (1.0f - mix) + y * mix;
    }

    void reset() override
    {
        prevX = 0.0f;
        prevY = 0.0f;
    }

private:
    void recalc()
    {
        const float fc = clampf(params.cutoffHz, 20.0f, sampleRate * 0.45f);
        const float rc = 1.0f / (2.0f * kPi * fc);
        const float dt = 1.0f / sampleRate;
        alpha = rc / (rc + dt);
        mix = clampf(params.mix, 0.0f, 1.0f);
    }

    float sampleRate;
    EffectParameters params;
    float alpha = 0.95f;
    float mix = 1.0f;
    float prevX = 0.0f;
    float prevY = 0.0f;
};

class DelayUnit final : public IEffectUnit {
public:
    DelayUnit(float sr, float maxDelaySeconds)
        : sampleRate(sr)
    {
        resizeBuffer(maxDelaySeconds);
    }

    void setSampleRate(float sr) override
    {
        sampleRate = sr;
        recalc();
    }

    void setParameters(const EffectParameters& p) override
    {
        params = p;
        recalc();
    }

    void setTempoBpm(float bpm) override
    {
        tempoBpm = std::max(1.0f, bpm);
        recalc();
    }

    float process(float input) override
    {
        if(!params.enabled || buffer.empty()) {
            return input;
        }
        const std::size_t readIndex = (writeIndex + buffer.size() - delaySamples) % buffer.size();
        const float delayed = buffer[readIndex];
        buffer[writeIndex] = input + delayed * feedback;
        writeIndex = (writeIndex + 1) % buffer.size();
        // Keep dry signal immediate; `mix` scales only the delayed repeats.
        return input + delayed * mix;
    }

    void reset() override
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

private:
    void resizeBuffer(float maxDelaySeconds)
    {
        const float clamped = std::max(0.05f, maxDelaySeconds);
        const std::size_t size = static_cast<std::size_t>(sampleRate * clamped) + 1;
        buffer.assign(size, 0.0f);
        writeIndex = 0;
        recalc();
    }

    void recalc()
    {
        if(buffer.empty()) {
            delaySamples = 1;
        } else {
            float computedDelayMs = params.delayMs;
            if(params.delayTempoNote != DelayTempoNote::Off){
                const float quarterNoteMs = 60000.0f / tempoBpm;
                float quarterNoteFactor = 1.0f;
                switch(params.delayTempoNote){
                    case DelayTempoNote::Off: quarterNoteFactor = 1.0f; break;
                    case DelayTempoNote::Quarter: quarterNoteFactor = 1.0f; break;
                    case DelayTempoNote::Half: quarterNoteFactor = 2.0f; break;
                    case DelayTempoNote::Whole: quarterNoteFactor = 4.0f; break;
                    case DelayTempoNote::Eighth: quarterNoteFactor = 0.5f; break;
                    case DelayTempoNote::Sixteenth: quarterNoteFactor = 0.25f; break;
                    case DelayTempoNote::COUNT: quarterNoteFactor = 1.0f; break;
                }
                computedDelayMs = quarterNoteMs * quarterNoteFactor;
            } else if(params.delaySyncToTempo){
                const float quarterNoteMs = 60000.0f / tempoBpm;
                float quarterNoteFactor = 1.0f;
                switch(params.delayDivision){
                    case DelayNoteDivision::ThirtySecond: quarterNoteFactor = 0.125f; break;
                    case DelayNoteDivision::Sixteenth: quarterNoteFactor = 0.25f; break;
                    case DelayNoteDivision::Eighth: quarterNoteFactor = 0.5f; break;
                    case DelayNoteDivision::Quarter: quarterNoteFactor = 1.0f; break;
                    case DelayNoteDivision::Half: quarterNoteFactor = 2.0f; break;
                    case DelayNoteDivision::Whole: quarterNoteFactor = 4.0f; break;
                    case DelayNoteDivision::DottedQuarter: quarterNoteFactor = 1.5f; break;
                    case DelayNoteDivision::QuarterTriplet: quarterNoteFactor = 2.0f / 3.0f; break;
                    case DelayNoteDivision::COUNT: quarterNoteFactor = 1.0f; break;
                }
                computedDelayMs = quarterNoteMs * quarterNoteFactor;
            }
            const float clampedMs = std::max(1.0f, computedDelayMs);
            delaySamples = static_cast<std::size_t>((clampedMs * 0.001f) * sampleRate);
            delaySamples = std::max<std::size_t>(1, std::min(delaySamples, buffer.size() - 1));
        }
        feedback = clampf(params.feedback, 0.0f, 0.98f);
        mix = clampf(params.mix, 0.0f, 1.0f);
    }

    float sampleRate;
    EffectParameters params;
    std::vector<float> buffer;
    std::size_t writeIndex = 0;
    std::size_t delaySamples = 1;
    float tempoBpm = 120.0f;
    float feedback = 0.25f;
    float mix = 0.5f;
};

class SchroederReverbUnit final : public IEffectUnit {
public:
    explicit SchroederReverbUnit(float sr) : sampleRate(sr)
    {
        allocateBuffers();
        recalc();
    }

    void setSampleRate(float sr) override
    {
        sampleRate = sr;
        allocateBuffers();
        recalc();
    }

    void setParameters(const EffectParameters& p) override
    {
        params = p;
        recalc();
    }

    float process(float input) override
    {
        if(!params.enabled) {
            return input;
        }

        float x = input;
        float combSum = 0.0f;
        for(std::size_t i = 0; i < combBuffers.size(); ++i) {
            combSum += processComb(i, x);
        }
        float y = combSum * 0.25f;
        for(std::size_t i = 0; i < allpassBuffers.size(); ++i) {
            y = processAllpass(i, y);
        }
        return input * (1.0f - mix) + y * mix;
    }

    void reset() override
    {
        for(std::size_t i = 0; i < combBuffers.size(); ++i) {
            std::fill(combBuffers[i].begin(), combBuffers[i].end(), 0.0f);
            combIndices[i] = 0;
            combFilterStore[i] = 0.0f;
        }
        for(std::size_t i = 0; i < allpassBuffers.size(); ++i) {
            std::fill(allpassBuffers[i].begin(), allpassBuffers[i].end(), 0.0f);
            allpassIndices[i] = 0;
        }
    }

private:
    void allocateBuffers()
    {
        static const int combTuning[4] = {1116, 1188, 1277, 1356};
        static const int allpassTuning[2] = {556, 441};
        const float scale = sampleRate / 44100.0f;

        combBuffers.clear();
        allpassBuffers.clear();
        combIndices.assign(4, 0);
        allpassIndices.assign(2, 0);
        combFilterStore.assign(4, 0.0f);

        for(int i = 0; i < 4; ++i) {
            int len = std::max(8, static_cast<int>(combTuning[i] * scale));
            combBuffers.push_back(std::vector<float>(static_cast<std::size_t>(len), 0.0f));
        }
        for(int i = 0; i < 2; ++i) {
            int len = std::max(8, static_cast<int>(allpassTuning[i] * scale));
            allpassBuffers.push_back(std::vector<float>(static_cast<std::size_t>(len), 0.0f));
        }
    }

    void recalc()
    {
        roomFeedback = 0.25f + clampf(params.roomSize, 0.0f, 1.0f) * 0.70f;
        damp1 = clampf(params.damping, 0.0f, 1.0f);
        damp2 = 1.0f - damp1;
        mix = clampf(params.mix, 0.0f, 1.0f);
    }

    float processComb(std::size_t idx, float input)
    {
        std::vector<float>& buffer = combBuffers[idx];
        std::size_t& wi = combIndices[idx];
        float output = buffer[wi];
        combFilterStore[idx] = output * damp2 + combFilterStore[idx] * damp1;
        buffer[wi] = input + combFilterStore[idx] * roomFeedback;
        wi = (wi + 1) % buffer.size();
        return output;
    }

    float processAllpass(std::size_t idx, float input)
    {
        std::vector<float>& buffer = allpassBuffers[idx];
        std::size_t& wi = allpassIndices[idx];
        float bufout = buffer[wi];
        float output = -input + bufout;
        buffer[wi] = input + bufout * 0.5f;
        wi = (wi + 1) % buffer.size();
        return output;
    }

    float sampleRate;
    EffectParameters params;

    std::vector<std::vector<float>> combBuffers;
    std::vector<std::vector<float>> allpassBuffers;
    std::vector<std::size_t> combIndices;
    std::vector<std::size_t> allpassIndices;
    std::vector<float> combFilterStore;

    float roomFeedback = 0.7f;
    float damp1 = 0.35f;
    float damp2 = 0.65f;
    float mix = 0.3f;
};

std::unique_ptr<IEffectUnit> makeUnit(EffectType type, float sampleRate, float maxDelaySeconds)
{
    if(type == EffectType::LowPass) {
        return std::unique_ptr<IEffectUnit>(new OnePoleLowPass(sampleRate));
    }
    if(type == EffectType::HighPass) {
        return std::unique_ptr<IEffectUnit>(new OnePoleHighPass(sampleRate));
    }
    if(type == EffectType::Delay) {
        return std::unique_ptr<IEffectUnit>(new DelayUnit(sampleRate, maxDelaySeconds));
    }
    if(type == EffectType::Reverb) {
        return std::unique_ptr<IEffectUnit>(new SchroederReverbUnit(sampleRate));
    }
    throw std::runtime_error("Unsupported effect type");
}
} // namespace

struct EffectsChain::EffectStage {
    int id = -1;
    EffectType type = EffectType::LowPass;
    EffectParameters params;
    std::unique_ptr<IEffectUnit> unit;
};

EffectsChain::EffectsChain(float sampleRate, float maxDelaySeconds)
    : sampleRate(sampleRate),
      maxDelaySeconds(maxDelaySeconds)
{
    if(sampleRate <= 1000.0f) {
        throw std::runtime_error("EffectsChain: invalid sample rate");
    }
    if(maxDelaySeconds <= 0.0f) {
        throw std::runtime_error("EffectsChain: invalid maxDelaySeconds");
    }
}

EffectsChain::~EffectsChain() = default;
EffectsChain::EffectsChain(EffectsChain&&) noexcept = default;
EffectsChain& EffectsChain::operator=(EffectsChain&&) noexcept = default;

void EffectsChain::setSampleRate(float newSampleRate)
{
    if(newSampleRate <= 1000.0f) {
        throw std::runtime_error("EffectsChain::setSampleRate invalid rate");
    }
    sampleRate = newSampleRate;
    for(std::size_t i = 0; i < stages.size(); ++i) {
        stages[i].unit->setSampleRate(sampleRate);
        stages[i].unit->setTempoBpm(tempoBpm);
        stages[i].unit->setParameters(stages[i].params);
    }
}

void EffectsChain::setTempoBpm(float bpm)
{
    tempoBpm = std::max(1.0f, bpm);
    for(std::size_t i = 0; i < stages.size(); ++i) {
        stages[i].unit->setTempoBpm(tempoBpm);
        stages[i].unit->setParameters(stages[i].params);
    }
}

int EffectsChain::addEffect(EffectType type, const EffectParameters& params)
{
    EffectStage stage;
    stage.id = nextEffectId++;
    stage.type = type;
    stage.params = params;
    stage.unit = makeUnit(type, sampleRate, maxDelaySeconds);
    stage.unit->setTempoBpm(tempoBpm);
    stage.unit->setParameters(params);
    stages.push_back(std::move(stage));
    return stages.back().id;
}

bool EffectsChain::removeEffect(int effectId)
{
    for(std::size_t i = 0; i < stages.size(); ++i) {
        if(stages[i].id == effectId) {
            stages.erase(stages.begin() + static_cast<long>(i));
            return true;
        }
    }
    return false;
}

bool EffectsChain::moveEffect(int effectId, std::size_t newIndex)
{
    if(newIndex >= stages.size()) {
        return false;
    }
    for(std::size_t i = 0; i < stages.size(); ++i) {
        if(stages[i].id == effectId) {
            if(i == newIndex) {
                return true;
            }
            EffectStage moved = std::move(stages[i]);
            stages.erase(stages.begin() + static_cast<long>(i));
            if(i < newIndex) {
                --newIndex;
            }
            stages.insert(stages.begin() + static_cast<long>(newIndex), std::move(moved));
            return true;
        }
    }
    return false;
}

bool EffectsChain::clearEffects()
{
    stages.clear();
    return true;
}

bool EffectsChain::setEffectParameters(int effectId, const EffectParameters& params)
{
    EffectStage* stage = findStage(effectId);
    if(!stage) {
        return false;
    }
    stage->params = params;
    stage->unit->setParameters(stage->params);
    return true;
}

bool EffectsChain::setEffectEnabled(int effectId, bool enabled)
{
    EffectStage* stage = findStage(effectId);
    if(!stage) {
        return false;
    }
    stage->params.enabled = enabled;
    stage->unit->setParameters(stage->params);
    return true;
}

bool EffectsChain::getEffectDescriptor(int effectId, EffectDescriptor& out) const
{
    const EffectStage* stage = findStage(effectId);
    if(!stage) {
        return false;
    }
    out.id = stage->id;
    out.type = stage->type;
    out.params = stage->params;
    return true;
}

std::vector<EffectDescriptor> EffectsChain::getEffects() const
{
    std::vector<EffectDescriptor> out;
    out.reserve(stages.size());
    for(std::size_t i = 0; i < stages.size(); ++i) {
        EffectDescriptor d;
        d.id = stages[i].id;
        d.type = stages[i].type;
        d.params = stages[i].params;
        out.push_back(d);
    }
    return out;
}

float EffectsChain::processSample(float input)
{
    float v = input;
    for(std::size_t i = 0; i < stages.size(); ++i) {
        v = stages[i].unit->process(v);
    }
    return v;
}

void EffectsChain::processBuffer(float* buffer, std::size_t numSamples)
{
    if(!buffer) {
        return;
    }
    for(std::size_t i = 0; i < numSamples; ++i) {
        buffer[i] = processSample(buffer[i]);
    }
}

void EffectsChain::resetState()
{
    for(std::size_t i = 0; i < stages.size(); ++i) {
        stages[i].unit->reset();
    }
}

EffectsChain::EffectStage* EffectsChain::findStage(int effectId)
{
    for(std::size_t i = 0; i < stages.size(); ++i) {
        if(stages[i].id == effectId) {
            return &stages[i];
        }
    }
    return nullptr;
}

const EffectsChain::EffectStage* EffectsChain::findStage(int effectId) const
{
    for(std::size_t i = 0; i < stages.size(); ++i) {
        if(stages[i].id == effectId) {
            return &stages[i];
        }
    }
    return nullptr;
}

EffectsBank::EffectsBank(float sampleRate, float maxDelaySeconds)
    : sampleRate(sampleRate),
      maxDelaySeconds(maxDelaySeconds)
{
    if(sampleRate <= 1000.0f) {
        throw std::runtime_error("EffectsBank: invalid sample rate");
    }
    if(maxDelaySeconds <= 0.0f) {
        throw std::runtime_error("EffectsBank: invalid maxDelaySeconds");
    }
}

void EffectsBank::setSampleRate(float newSampleRate)
{
    if(newSampleRate <= 1000.0f) {
        throw std::runtime_error("EffectsBank::setSampleRate invalid rate");
    }
    sampleRate = newSampleRate;
    for(std::unordered_map<std::string, EffectsChain>::iterator it = chains.begin(); it != chains.end(); ++it) {
        it->second.setSampleRate(sampleRate);
    }
}

bool EffectsBank::hasChain(const std::string& chainId) const
{
    return chains.find(chainId) != chains.end();
}

EffectsChain& EffectsBank::getOrCreateChain(const std::string& chainId)
{
    std::unordered_map<std::string, EffectsChain>::iterator it = chains.find(chainId);
    if(it != chains.end()) {
        return it->second;
    }
    std::pair<std::unordered_map<std::string, EffectsChain>::iterator, bool> inserted =
        chains.emplace(chainId, EffectsChain(sampleRate, maxDelaySeconds));
    return inserted.first->second;
}

const EffectsChain* EffectsBank::getChainIfExists(const std::string& chainId) const
{
    std::unordered_map<std::string, EffectsChain>::const_iterator it = chains.find(chainId);
    if(it == chains.end()) {
        return nullptr;
    }
    return &it->second;
}

bool EffectsBank::removeChain(const std::string& chainId)
{
    return chains.erase(chainId) > 0;
}

void EffectsBank::clear()
{
    chains.clear();
}
