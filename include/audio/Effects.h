#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

enum class EffectType {
    /// One-pole low-pass filter.
    LowPass,
    /// One-pole high-pass filter.
    HighPass,
    /// Non-linear saturation / drive.
    Drive,
    /// Modulated short delay for width and movement.
    Chorus,
    /// Sweeping all-pass notch effect.
    Phaser,
    /// Feedback delay line.
    Delay,
    /// Schroeder-style reverb.
    Reverb
};

enum class DelayNoteDivision {
    ThirtySecond = 0,
    Sixteenth,
    Eighth,
    Quarter,
    Half,
    Whole,
    DottedQuarter,
    QuarterTriplet,
    COUNT
};

enum class DelayTempoNote {
    Off = 0,
    Quarter,
    Half,
    Whole,
    Eighth,
    Sixteenth,
    COUNT
};

/// Parameter set shared by all effect types.
struct EffectParameters {
    bool enabled = true;
    float mix = 1.0f;          // 0 = dry, 1 = wet

    // LowPass / HighPass
    float cutoffHz = 1200.0f;
    float resonance = 0.0f;    // [0..1], mapped internally to filter Q

    // Drive / modulation effects
    float drive = 4.0f;        // drive amount (linear pre-gain)
    float rateHz = 0.6f;       // modulation rate
    float depth = 0.5f;        // modulation depth [0..1]

    // Delay
    float delayMs = 250.0f;
    float feedback = 0.25f;    // [0..0.98]
    bool delaySyncToTempo = false;
    DelayNoteDivision delayDivision = DelayNoteDivision::Quarter;
    DelayTempoNote delayTempoNote = DelayTempoNote::Off;

    // Reverb
    float roomSize = 0.55f;    // [0..1]
    float damping = 0.35f;     // [0..1]
};

/// Public metadata for one stage in an effects chain.
struct EffectDescriptor {
    int id = -1;
    EffectType type = EffectType::LowPass;
    EffectParameters params;
};

/// Ordered chain of mono effects processing one sample stream.
class EffectsChain {
public:
    /// Create a chain for the given sample rate and max delay memory size.
    explicit EffectsChain(float sampleRate = 44100.0f, float maxDelaySeconds = 4.0f);
    ~EffectsChain();
    EffectsChain(EffectsChain&&) noexcept;
    EffectsChain& operator=(EffectsChain&&) noexcept;
    EffectsChain(const EffectsChain&) = delete;
    EffectsChain& operator=(const EffectsChain&) = delete;

    /// Update sample rate for all stages in this chain.
    void setSampleRate(float sampleRate);
    /// Update tempo (BPM) used by tempo-synced effects (e.g. delay sync).
    void setTempoBpm(float bpm);
    /// Return the chain sample rate.
    float getSampleRate() const { return sampleRate; }

    /// Append an effect stage and return its stable stage id.
    int addEffect(EffectType type, const EffectParameters& params = EffectParameters{});
    /// Remove a stage by id. Returns false if id is not found.
    bool removeEffect(int effectId);
    /// Move a stage by id to a new index. Returns false on invalid id/index.
    bool moveEffect(int effectId, std::size_t newIndex);
    /// Remove all stages.
    bool clearEffects();

    /// Replace all parameters for one stage.
    bool setEffectParameters(int effectId, const EffectParameters& params);
    /// Enable/disable one stage without changing other parameters.
    bool setEffectEnabled(int effectId, bool enabled);
    /// Fetch metadata for one stage.
    bool getEffectDescriptor(int effectId, EffectDescriptor& out) const;
    /// Return a snapshot of all stages in processing order.
    std::vector<EffectDescriptor> getEffects() const;

    /// Process one mono sample through the full chain.
    float processSample(float input);
    /// Process an in-place mono buffer through the full chain.
    void processBuffer(float* buffer, std::size_t numSamples);
    /// Reset all internal delay/filter states.
    void resetState();

private:
    struct EffectStage;

    EffectStage* findStage(int effectId);
    const EffectStage* findStage(int effectId) const;

    float sampleRate;
    float tempoBpm = 120.0f;
    float maxDelaySeconds;
    int nextEffectId = 1;
    std::vector<EffectStage> stages;
};

/// Named collection of independent effects chains (for separate signal targets).
class EffectsBank {
public:
    /// Create a bank. Newly created chains inherit these defaults.
    explicit EffectsBank(float sampleRate = 44100.0f, float maxDelaySeconds = 4.0f);

    /// Update sample rate for all existing chains.
    void setSampleRate(float sampleRate);
    /// Return the bank sample rate.
    float getSampleRate() const { return sampleRate; }

    /// Check if a named chain exists.
    bool hasChain(const std::string& chainId) const;
    /// Return existing chain or create a new one with default settings.
    EffectsChain& getOrCreateChain(const std::string& chainId);
    /// Return pointer to a chain if present, otherwise nullptr.
    const EffectsChain* getChainIfExists(const std::string& chainId) const;
    /// Remove one named chain.
    bool removeChain(const std::string& chainId);
    /// Remove all chains.
    void clear();

private:
    float sampleRate;
    float maxDelaySeconds;
    std::unordered_map<std::string, EffectsChain> chains;
};
