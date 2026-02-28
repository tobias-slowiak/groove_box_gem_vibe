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
  float release = 0.08f;
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
  std::string folderName;  // SamplePack selection token (instrument_id)
  InstrumentDefaults defaults;
};

class InstrumentCatalog {
public:
    InstrumentCatalog();

    std::string& getDisplayName(size_t index);

    std::string& getFolderName(size_t index);

    InstrumentDefaults& getDefaults(size_t index);

    int size(){return catalog.size();}

private:
    void loadFromVCSLTables();
    std::vector<InstrumentInfo> catalog;
};

class DrumCatalog {
public:
    DrumCatalog();

    std::string& getDisplayName(size_t index);

    std::string& getFolderName(size_t index);

    InstrumentDefaults& getDefaults(size_t index);

    int size(){return catalog.size();}
private:
    void loadFromVCSLTables();
    std::vector<InstrumentInfo> catalog;
};
