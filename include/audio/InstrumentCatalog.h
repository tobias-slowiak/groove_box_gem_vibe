// include/audio/InstrumentCatalog.h
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Effects.h"

struct InstrumentVoiceDefaults {
  float attack = 0.0f;
  float decay = 0.0f;
  float sustain = 1.0f;
  float release = 0.1f;
  bool repeat = false;
};

struct EffectStageDefaults {
  EffectType type = EffectType::LowPass;
  EffectParameters params;
};

struct InstrumentDefaults {
  InstrumentVoiceDefaults voice;
  std::vector<EffectStageDefaults> effects;
};

struct InstrumentInfo {
  std::string displayName; // UI label
  std::string folderName;  // SD card folder
  InstrumentDefaults defaults;
};

class InstrumentCatalog {
public:
    std::string& getDisplayName(size_t index);

    std::string& getFolderName(size_t index);

    InstrumentDefaults& getDefaults(size_t index);

    int size(){return catalog.size();}

private:
    static EffectStageDefaults makeFilterDefaults(EffectType type, float cutoffHz, float resonance, float mix){
        EffectParameters p;
        p.mix = mix;
        p.cutoffHz = cutoffHz;
        p.resonance = resonance;
        return {type, p};
    }

    std::vector<InstrumentInfo> catalog =
    {
        {"Piano", "/root/Bela/Samples/Piano",
            {{0.005f, 0.08f, 0.85f, 0.20f, false}, {}}},
        {"Std_Bass", "/root/Bela/Samples/Standard_Bass",
            {{0.003f, 0.10f, 0.80f, 0.20f, false}, {}}},
        {"Sine", "/root/Bela/Samples/SineOscillator",
            {{0.010f, 0.05f, 0.85f, 0.20f, false}, {}}},
        {"Saw", "/root/Bela/Samples/SawOscillator",
            {{0.004f, 0.08f, 0.75f, 0.15f, false}, {}}},
        {"Square", "/root/Bela/Samples/SquareOscillator",
            {{0.002f, 0.06f, 0.80f, 0.18f, false}, {}}},
        {"Electronic_Accordion", "/root/Bela/Samples/SquareOscillator",
            {{0.000f, 0.03f, 0.95f, 0.12f, true},
                {
                    makeFilterDefaults(EffectType::HighPass, 180.0f, 0.45f, 1.0f),
                    makeFilterDefaults(EffectType::LowPass, 2600.0f, 0.60f, 1.0f)
                }}}
    };
};

class DrumCatalog {
public:
    std::string& getDisplayName(size_t index);

    std::string& getFolderName(size_t index);

    InstrumentDefaults& getDefaults(size_t index);

    int size(){return catalog.size();}
private:
    std::vector<InstrumentInfo> catalog =
    {
        {"808", "/root/Bela/Samples/drumkit_808",
            {{0.0f, 0.0f, 1.0f, 0.06f, false}, {}}}
    };
};
