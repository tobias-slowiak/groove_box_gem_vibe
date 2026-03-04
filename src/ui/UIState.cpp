 #include "../../include/ui/UI.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/audio/Loopers.h"
#include "../../include/audio/Effects.h"
 #include <string>
 #include <algorithm>
#include <cctype>
 #include <functional>
 //compile

UIStateContext::UIStateContext(ResourceManager& rm): rm(rm), metronome(rm.getMetronome()), loopers(rm.getLoopers()) {
    instrumentSetIndex = rm.getInstrumentSetLibrary().getActiveSetIndex();
    setNameDraft = rm.getInstrumentSetLibrary().getSetName(instrumentSetIndex);
}

std::string getLooperTriggerModeString(LooperTriggerMode mode) {
    switch (mode) {
        case LooperTriggerMode::OnBar:
            return "OnBar";
        case LooperTriggerMode::Free:
            return "Free";
        default:
            return "Unknown";
    }
}

float clampFx(float value, float lo, float hi){
    return std::max(lo, std::min(value, hi));
}

int getEffectsTargetCount(UIStateContext& ctxt){
    return 2 + ctxt.loopers.size(); // Input, Instrument, Looper0..N
}

std::string getEffectsTargetName(UIStateContext&, int targetIndex){
    if(targetIndex == 0) return "Input";
    if(targetIndex == 1) return "Instr";
    return "Looper" + std::to_string(targetIndex - 2);
}

EffectType effectTypeFromIndex(int effectTypeIndex){
    switch(effectTypeIndex){
        case 0: return EffectType::LowPass;
        case 1: return EffectType::HighPass;
        case 2: return EffectType::Drive;
        case 3: return EffectType::Chorus;
        case 4: return EffectType::Phaser;
        case 5: return EffectType::Delay;
        case 6: return EffectType::Reverb;
        default: return EffectType::LowPass;
    }
}

std::string effectTypeToString(EffectType type){
    switch(type){
        case EffectType::LowPass: return "LP";
        case EffectType::HighPass: return "HP";
        case EffectType::Drive: return "Drive";
        case EffectType::Chorus: return "Chorus";
        case EffectType::Phaser: return "Phaser";
        case EffectType::Delay: return "Delay";
        case EffectType::Reverb: return "Reverb";
        default: return "FX";
    }
}

std::string delayTempoNoteToString(DelayTempoNote note){
    switch(note){
        case DelayTempoNote::Off: return "Off";
        case DelayTempoNote::Quarter: return "1/4";
        case DelayTempoNote::Half: return "1/2";
        case DelayTempoNote::Whole: return "1/1";
        case DelayTempoNote::Eighth: return "1/8";
        case DelayTempoNote::Sixteenth: return "1/16";
        case DelayTempoNote::COUNT: return "Off";
    }
    return "Off";
}

DelayTempoNote cycleDelayTempoNote(DelayTempoNote note, int shift){
    int count = static_cast<int>(DelayTempoNote::COUNT);
    int idx = (static_cast<int>(note) + shift) % count;
    if(idx < 0) idx += count;
    return static_cast<DelayTempoNote>(idx);
}

float delayTempoNoteToMs(DelayTempoNote note, float bpm){
    const float clampedBpm = std::max(1.0f, bpm);
    const float quarterMs = 60000.0f / clampedBpm;
    switch(note){
        case DelayTempoNote::Quarter: return quarterMs;
        case DelayTempoNote::Half: return quarterMs * 2.0f;
        case DelayTempoNote::Whole: return quarterMs * 4.0f;
        case DelayTempoNote::Eighth: return quarterMs * 0.5f;
        case DelayTempoNote::Sixteenth: return quarterMs * 0.25f;
        case DelayTempoNote::Off:
        case DelayTempoNote::COUNT:
            return 0.0f;
    }
    return 0.0f;
}

EffectsChain& getDisplayEffectsChain(UIStateContext& ctxt){
    int targetIndex = ctxt.effectsTargetIndex;
    if(targetIndex == 0){
        return ctxt.rm.getSignalRouter().getInputEffects(0);
    }
    if(targetIndex == 1){
        return ctxt.rm.getSignalRouter().getInstrumentEffects();
    }
    return ctxt.rm.getSignalRouter().getLooperEffects(targetIndex - 2);
}

void forEachEffectsChainOnTarget(UIStateContext& ctxt, const std::function<void(EffectsChain&)>& fn){
    int targetIndex = ctxt.effectsTargetIndex;
    if(targetIndex == 0){
        fn(ctxt.rm.getSignalRouter().getInputEffects(0));
        fn(ctxt.rm.getSignalRouter().getInputEffects(1));
        return;
    }
    if(targetIndex == 1){
        fn(ctxt.rm.getSignalRouter().getInstrumentEffects());
        return;
    }
    fn(ctxt.rm.getSignalRouter().getLooperEffects(targetIndex - 2));
}

void clampEffectsSelection(UIStateContext& ctxt){
    int targetCount = getEffectsTargetCount(ctxt);
    if(targetCount <= 0){
        ctxt.effectsTargetIndex = 0;
    } else {
        int targetIndex = ctxt.effectsTargetIndex % targetCount;
        if(targetIndex < 0) targetIndex += targetCount;
        ctxt.effectsTargetIndex = targetIndex;
    }

    int effectTypeCount = 7;
    int effectTypeIndex = ctxt.effectsNewTypeIndex % effectTypeCount;
    if(effectTypeIndex < 0) effectTypeIndex += effectTypeCount;
    ctxt.effectsNewTypeIndex = effectTypeIndex;

    std::vector<EffectDescriptor> effects = getDisplayEffectsChain(ctxt).getEffects();
    if(effects.empty()){
        ctxt.effectsStageIndex = 0;
        return;
    }
    int stageIndex = ctxt.effectsStageIndex % static_cast<int>(effects.size());
    if(stageIndex < 0) stageIndex += static_cast<int>(effects.size());
    ctxt.effectsStageIndex = stageIndex;
}

bool getSelectedEffectDescriptor(UIStateContext& ctxt, EffectDescriptor& out){
    clampEffectsSelection(ctxt);
    std::vector<EffectDescriptor> effects = getDisplayEffectsChain(ctxt).getEffects();
    if(effects.empty()){
        return false;
    }
    out = effects.at(ctxt.effectsStageIndex);
    return true;
}

void applyToSelectedEffect(UIStateContext& ctxt, const std::function<void(EffectParameters&, EffectType)>& updater){
    clampEffectsSelection(ctxt);
    forEachEffectsChainOnTarget(ctxt, [&](EffectsChain& chain){
        std::vector<EffectDescriptor> effects = chain.getEffects();
        if(effects.empty()) return;
        int stageIndex = ctxt.effectsStageIndex;
        if(stageIndex >= static_cast<int>(effects.size())){
            stageIndex = static_cast<int>(effects.size()) - 1;
        }
        EffectDescriptor desc = effects.at(stageIndex);
        EffectParameters params = desc.params;
        updater(params, desc.type);
        chain.setEffectParameters(desc.id, params);
    });
}

void addEffectOnCurrentTarget(UIStateContext& ctxt){
    clampEffectsSelection(ctxt);
    EffectType type = effectTypeFromIndex(ctxt.effectsNewTypeIndex);
    forEachEffectsChainOnTarget(ctxt, [&](EffectsChain& chain){
        chain.addEffect(type);
    });
    std::vector<EffectDescriptor> effects = getDisplayEffectsChain(ctxt).getEffects();
    if(!effects.empty()){
        ctxt.effectsStageIndex = static_cast<int>(effects.size()) - 1;
    }
}

void removeSelectedEffectOnCurrentTarget(UIStateContext& ctxt){
    clampEffectsSelection(ctxt);
    forEachEffectsChainOnTarget(ctxt, [&](EffectsChain& chain){
        std::vector<EffectDescriptor> effects = chain.getEffects();
        if(effects.empty()) return;
        int stageIndex = ctxt.effectsStageIndex;
        if(stageIndex >= static_cast<int>(effects.size())){
            stageIndex = static_cast<int>(effects.size()) - 1;
        }
        chain.removeEffect(effects.at(stageIndex).id);
    });
    clampEffectsSelection(ctxt);
}

SamplePackVoiceSettings toVoiceSettings(const InstrumentDefaults& defaults){
    SamplePackVoiceSettings out;
    out.attack = defaults.voice.attack;
    out.decay = defaults.voice.decay;
    out.sustain = defaults.voice.sustain;
    out.release = defaults.voice.release;
    out.repeat = defaults.voice.repeat;
    return out;
}

void applyEffectDefaultsToInstrumentBus(UIStateContext& ctxt, const std::vector<EffectStageDefaults>& defaults){
    EffectsChain& chain = ctxt.rm.getSignalRouter().getInstrumentEffects();
    chain.clearEffects();
    for(const auto& effect : defaults){
        chain.addEffect(effect.type, effect.params);
    }
}

void applyCatalogDefaults(UIStateContext& ctxt, bool keys, bool reloadSamples){
    if(keys){
        auto& catalog = ctxt.rm.getInstrumentCatalog();
        SamplePack& samplePack = ctxt.rm.getKeyInstrumentSamplePack();
        if(reloadSamples){
            samplePack.initForFolder(catalog.getFolderName(ctxt.instrumentIndex));
        }
        InstrumentDefaults& defaults = catalog.getDefaults(ctxt.instrumentIndex);
        samplePack.setVoiceSettings(toVoiceSettings(defaults));
        if(ctxt.rm.keysInMelodicMode){
            applyEffectDefaultsToInstrumentBus(ctxt, defaults.effects);
        }
        return;
    }

    auto& catalog = ctxt.rm.getDrumCatalog();
    SamplePack& samplePack = ctxt.rm.getDrumSamplePack();
    if(reloadSamples || !samplePack.hasLoadedSamples()){
        samplePack.initForFolder(catalog.getFolderName(ctxt.drumIndex));
    }
    InstrumentDefaults& defaults = catalog.getDefaults(ctxt.drumIndex);
    samplePack.setVoiceSettings(toVoiceSettings(defaults));
    if(!ctxt.rm.keysInMelodicMode){
        applyEffectDefaultsToInstrumentBus(ctxt, defaults.effects);
    }
}

int findCatalogIndexByFolderName(InstrumentCatalog& catalog, const std::string& folderName){
    for(int i = 0; i < catalog.size(); ++i){
        if(catalog.getFolderName(static_cast<size_t>(i)) == folderName){
            return i;
        }
    }
    return -1;
}

int clampSetSelection(UIStateContext& ctxt, int index){
    return ctxt.rm.getInstrumentSetLibrary().clampSetIndex(index);
}

std::string trimTokenCopy(const std::string& in){
    size_t start = 0;
    while(start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))){
        ++start;
    }
    size_t end = in.size();
    while(end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))){
        --end;
    }
    return in.substr(start, end - start);
}

const std::string& editorAlphabet(){
    static const std::string kAlphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    return kAlphabet;
}

void ensureCursorAndText(std::string& text, int& cursor){
    if(text.empty()){
        text = " ";
    }
    if(cursor < 0){
        cursor = 0;
    }
    if(cursor >= static_cast<int>(text.size())){
        cursor = static_cast<int>(text.size()) - 1;
    }
}

void cycleTextChar(std::string& text, int& cursor, int direction){
    ensureCursorAndText(text, cursor);
    const std::string& alphabet = editorAlphabet();
    char c = text[static_cast<size_t>(cursor)];
    size_t pos = alphabet.find(c);
    if(pos == std::string::npos){
        pos = 0;
    }
    int next = static_cast<int>(pos) + direction;
    const int count = static_cast<int>(alphabet.size());
    next %= count;
    if(next < 0){
        next += count;
    }
    text[static_cast<size_t>(cursor)] = alphabet[static_cast<size_t>(next)];
}

void moveTextCursor(std::string& text, int& cursor, int direction){
    ensureCursorAndText(text, cursor);
    int next = cursor + direction;
    if(next < 0){
        next = 0;
    }
    if(next >= static_cast<int>(text.size())){
        next = static_cast<int>(text.size()) - 1;
    }
    cursor = next;
}

std::string withCursorIndicator(std::string text, int cursor){
    ensureCursorAndText(text, cursor);
    if(cursor >= 0 && cursor < static_cast<int>(text.size())){
        text[static_cast<size_t>(cursor)] = std::toupper(static_cast<unsigned char>(text[static_cast<size_t>(cursor)]));
    }
    return text;
}

void syncSetSelection(UIStateContext& ctxt){
    InstrumentSetLibrary& library = ctxt.rm.getInstrumentSetLibrary();
    ctxt.instrumentSetIndex = clampSetSelection(ctxt, ctxt.instrumentSetIndex);
    library.setActiveSetIndex(ctxt.instrumentSetIndex);
    ctxt.setEntryIndex = library.clampEntryIndex(ctxt.instrumentSetIndex, ctxt.setEntryIndex);
}

bool currentSetHasEntries(UIStateContext& ctxt){
    return ctxt.rm.getInstrumentSetLibrary().getEntryCount(ctxt.instrumentSetIndex) > 0;
}

std::string toLowerAscii(std::string value){
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool looksLikeDrumInstrument(const InstrumentSetEntry& entry){
    static const char* kNeedles[] = {
        "drum", "kit", "kick", "snare", "hihat", "hi-hat", "tom", "cymbal",
        "ride", "crash", "perc", "percussion", "clap", "rim", "tambourine", "shaker"
    };

    const std::string displayLower = toLowerAscii(entry.displayName);
    const std::string instrumentLower = toLowerAscii(entry.instrumentId);
    for(const char* needle : kNeedles){
        if(displayLower.find(needle) != std::string::npos){
            return true;
        }
        if(instrumentLower.find(needle) != std::string::npos){
            return true;
        }
    }
    return false;
}

bool findNextMelodicSetEntry(UIStateContext& ctxt, int startIndex, int direction, int& outIndex){
    InstrumentSetLibrary& library = ctxt.rm.getInstrumentSetLibrary();
    const int count = library.getEntryCount(ctxt.instrumentSetIndex);
    if(count <= 0){
        return false;
    }

    int step = (direction < 0) ? -1 : 1;
    int index = startIndex % count;
    if(index < 0){
        index += count;
    }

    for(int scanned = 0; scanned < count; ++scanned){
        const InstrumentSetEntry& entry = library.getEntry(ctxt.instrumentSetIndex, index);
        if(!looksLikeDrumInstrument(entry)){
            outIndex = index;
            return true;
        }
        index += step;
        if(index >= count){
            index = 0;
        } else if(index < 0){
            index = count - 1;
        }
    }
    return false;
}

bool alignKeySelectionToMelodicEntry(UIStateContext& ctxt){
    int melodicIndex = 0;
    if(!findNextMelodicSetEntry(ctxt, ctxt.setEntryIndex, +1, melodicIndex)){
        return false;
    }
    ctxt.setEntryIndex = melodicIndex;
    return true;
}

std::string getCurrentSetKeyDisplayName(UIStateContext& ctxt){
    InstrumentSetLibrary& library = ctxt.rm.getInstrumentSetLibrary();
    syncSetSelection(ctxt);
    if(!currentSetHasEntries(ctxt) || !alignKeySelectionToMelodicEntry(ctxt)){
        return ctxt.rm.getInstrumentCatalog().getDisplayName(ctxt.instrumentIndex);
    }
    const InstrumentSetEntry& entry = library.getEntry(ctxt.instrumentSetIndex, ctxt.setEntryIndex);
    if(!entry.displayName.empty()){
        return entry.displayName;
    }
    const int catalogIndex = findCatalogIndexByFolderName(ctxt.rm.getInstrumentCatalog(), entry.instrumentId);
    if(catalogIndex >= 0){
        return ctxt.rm.getInstrumentCatalog().getDisplayName(static_cast<size_t>(catalogIndex));
    }
    return entry.instrumentId;
}

void applySelectedKeyInstrument(UIStateContext& ctxt, bool reloadSamples){
    InstrumentSetLibrary& library = ctxt.rm.getInstrumentSetLibrary();
    syncSetSelection(ctxt);

    if(currentSetHasEntries(ctxt) && alignKeySelectionToMelodicEntry(ctxt)){
        const InstrumentSetEntry& entry = library.getEntry(ctxt.instrumentSetIndex, ctxt.setEntryIndex);
        const int catalogIndex = findCatalogIndexByFolderName(ctxt.rm.getInstrumentCatalog(), entry.instrumentId);
        if(catalogIndex >= 0){
            ctxt.instrumentIndex = catalogIndex;
        }
        SamplePack& samplePack = ctxt.rm.getKeyInstrumentSamplePack();
        if(reloadSamples){
            samplePack.initForFolder(entry.instrumentId);
        }
        if(catalogIndex >= 0){
            InstrumentDefaults& defaults = ctxt.rm.getInstrumentCatalog().getDefaults(static_cast<size_t>(catalogIndex));
            samplePack.setVoiceSettings(toVoiceSettings(defaults));
            if(ctxt.rm.keysInMelodicMode){
                applyEffectDefaultsToInstrumentBus(ctxt, defaults.effects);
            }
        }
        return;
    }

    applyCatalogDefaults(ctxt, true, reloadSamples);
}

void stepSelectedKeyInstrument(UIStateContext& ctxt, int direction){
    syncSetSelection(ctxt);
    if(currentSetHasEntries(ctxt)){
        const int step = (direction < 0) ? -1 : 1;
        int melodicIndex = 0;
        if(findNextMelodicSetEntry(ctxt, ctxt.setEntryIndex + step, step, melodicIndex)){
            ctxt.setEntryIndex = melodicIndex;
            return;
        }
    }

    const int count = ctxt.rm.getInstrumentCatalog().size();
    const int step = (direction < 0) ? -1 : 1;
    int next = ctxt.instrumentIndex + step;
    next %= count;
    if(next < 0){
        next += count;
    }
    ctxt.instrumentIndex = next;
}

SamplePack& getActiveModeSamplePack(UIStateContext& ctxt){
    if(ctxt.rm.keysInMelodicMode){
        return ctxt.rm.getKeyInstrumentSamplePack();
    }
    return ctxt.rm.getDrumSamplePack();
}

std::string getSelectedEffectParamDisplay(UIStateContext& ctxt, int paramIndex){
    EffectDescriptor desc;
    if(!getSelectedEffectDescriptor(ctxt, desc)){
        return std::string("-");
    }

    const EffectParameters& p = desc.params;
    if(desc.type == EffectType::LowPass || desc.type == EffectType::HighPass){
        if(paramIndex == 0) return "cut:" + std::to_string(static_cast<int>(p.cutoffHz));
        if(paramIndex == 1) return "mix:" + std::to_string(p.mix);
        if(paramIndex == 2) return "fine:" + std::to_string(static_cast<int>(p.cutoffHz));
        if(paramIndex == 3) return "res:" + std::to_string(p.resonance);
        return std::string("-");
    }

    if(desc.type == EffectType::Delay){
        if(paramIndex == 0){
            if(p.delayTempoNote != DelayTempoNote::Off){
                const float syncedMs = delayTempoNoteToMs(p.delayTempoNote, ctxt.metronome.getBPM());
                return "ms:" + std::to_string(static_cast<int>(syncedMs));
            }
            return "ms:" + std::to_string(static_cast<int>(p.delayMs));
        }
        if(paramIndex == 1) return "fb:" + std::to_string(p.feedback);
        if(paramIndex == 2) return "mix:" + std::to_string(p.mix);
        if(paramIndex == 3) return "note:" + delayTempoNoteToString(p.delayTempoNote);
        return std::string("-");
    }

    if(desc.type == EffectType::Drive){
        if(paramIndex == 0) return "drv:" + std::to_string(p.drive);
        if(paramIndex == 1) return "tone:" + std::to_string(static_cast<int>(p.cutoffHz));
        if(paramIndex == 2) return "shp:" + std::to_string(p.resonance);
        if(paramIndex == 3) return "mix:" + std::to_string(p.mix);
        return std::string("-");
    }

    if(desc.type == EffectType::Chorus){
        if(paramIndex == 0) return "rate:" + std::to_string(p.rateHz);
        if(paramIndex == 1) return "dep:" + std::to_string(p.depth);
        if(paramIndex == 2) return "dly:" + std::to_string(static_cast<int>(p.delayMs));
        if(paramIndex == 3) return "mix:" + std::to_string(p.mix);
        return std::string("-");
    }

    if(desc.type == EffectType::Phaser){
        if(paramIndex == 0) return "rate:" + std::to_string(p.rateHz);
        if(paramIndex == 1) return "dep:" + std::to_string(p.depth);
        if(paramIndex == 2) return "fb:" + std::to_string(p.resonance);
        if(paramIndex == 3) return "mix:" + std::to_string(p.mix);
        return std::string("-");
    }

    if(desc.type == EffectType::Reverb){
        if(paramIndex == 0) return "room:" + std::to_string(p.roomSize);
        if(paramIndex == 1) return "damp:" + std::to_string(p.damping);
        if(paramIndex == 2) return "mix:" + std::to_string(p.mix);
        if(paramIndex == 3) return "rFine:" + std::to_string(p.roomSize);
        return std::string("-");
    }

    return std::string("-");
}

void adjustSelectedEffectParam(UIStateContext& ctxt, int paramIndex, int direction){
    if(direction == 0){
        return;
    }
    const int shift = direction > 0 ? 1 : -1;
    applyToSelectedEffect(ctxt, [&](EffectParameters& p, EffectType type){
        if(type == EffectType::LowPass || type == EffectType::HighPass){
            if(paramIndex == 0){
                p.cutoffHz = clampFx(p.cutoffHz + (100.0f * static_cast<float>(shift)), 20.0f, 18000.0f);
            } else if(paramIndex == 1){
                p.mix = clampFx(p.mix + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 2){
                p.cutoffHz = clampFx(p.cutoffHz + (10.0f * static_cast<float>(shift)), 20.0f, 18000.0f);
            } else if(paramIndex == 3){
                p.resonance = clampFx(p.resonance + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            }
            return;
        }

        if(type == EffectType::Delay){
            if(paramIndex == 0){
                p.delayMs = clampFx(p.delayMs + (10.0f * static_cast<float>(shift)), 1.0f, 2000.0f);
                p.delayTempoNote = DelayTempoNote::Off;
                p.delaySyncToTempo = false;
            } else if(paramIndex == 1){
                p.feedback = clampFx(p.feedback + (0.05f * static_cast<float>(shift)), 0.0f, 0.98f);
            } else if(paramIndex == 2){
                p.mix = clampFx(p.mix + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 3){
                p.delayTempoNote = cycleDelayTempoNote(p.delayTempoNote, shift);
                if(p.delayTempoNote != DelayTempoNote::Off){
                    p.delaySyncToTempo = false;
                    p.delayMs = clampFx(delayTempoNoteToMs(p.delayTempoNote, ctxt.metronome.getBPM()), 1.0f, 2000.0f);
                }
            }
            return;
        }

        if(type == EffectType::Drive){
            if(paramIndex == 0){
                p.drive = clampFx(p.drive + (0.5f * static_cast<float>(shift)), 1.0f, 40.0f);
            } else if(paramIndex == 1){
                p.cutoffHz = clampFx(p.cutoffHz + (100.0f * static_cast<float>(shift)), 120.0f, 18000.0f);
            } else if(paramIndex == 2){
                p.resonance = clampFx(p.resonance + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 3){
                p.mix = clampFx(p.mix + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            }
            return;
        }

        if(type == EffectType::Chorus){
            if(paramIndex == 0){
                p.rateHz = clampFx(p.rateHz + (0.05f * static_cast<float>(shift)), 0.05f, 8.0f);
            } else if(paramIndex == 1){
                p.depth = clampFx(p.depth + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 2){
                p.delayMs = clampFx(p.delayMs + (1.0f * static_cast<float>(shift)), 2.0f, 30.0f);
            } else if(paramIndex == 3){
                p.mix = clampFx(p.mix + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            }
            return;
        }

        if(type == EffectType::Phaser){
            if(paramIndex == 0){
                p.rateHz = clampFx(p.rateHz + (0.05f * static_cast<float>(shift)), 0.05f, 6.0f);
            } else if(paramIndex == 1){
                p.depth = clampFx(p.depth + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 2){
                p.resonance = clampFx(p.resonance + (0.05f * static_cast<float>(shift)), 0.0f, 0.95f);
            } else if(paramIndex == 3){
                p.mix = clampFx(p.mix + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            }
            return;
        }

        if(type == EffectType::Reverb){
            if(paramIndex == 0){
                p.roomSize = clampFx(p.roomSize + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 1){
                p.damping = clampFx(p.damping + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 2){
                p.mix = clampFx(p.mix + (0.05f * static_cast<float>(shift)), 0.0f, 1.0f);
            } else if(paramIndex == 3){
                p.roomSize = clampFx(p.roomSize + (0.01f * static_cast<float>(shift)), 0.0f, 1.0f);
            }
        }
    });
}




UIState::UIState(UI& uiRef, UIStateId stateId)
     : id(stateId)
{
    paramLines.clear();
    subParamLines.clear();
    if(stateId == UIStateId::SetEditor){
        paramLines.push_back({"SetEdit",
                            []{return "";},
                                    {
                                        []() {},
                                        []() {},
                                        []() {}
                                    }
                            });
        subParamLines.push_back({
                            {"Set",
                            [&uiRef]() {
                                syncSetSelection(uiRef.ctxt);
                                return uiRef.ctxt.rm.getInstrumentSetLibrary().getSetName(uiRef.ctxt.instrumentSetIndex);
                            },
                                {
                                    [&uiRef]() {
                                        uiRef.ctxt.instrumentSetIndex = clampSetSelection(uiRef.ctxt, uiRef.ctxt.instrumentSetIndex + 1);
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().setActiveSetIndex(uiRef.ctxt.instrumentSetIndex);
                                        uiRef.ctxt.setEntryIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().clampEntryIndex(uiRef.ctxt.instrumentSetIndex, 0);
                                        uiRef.ctxt.setNameDraft = uiRef.ctxt.rm.getInstrumentSetLibrary().getSetName(uiRef.ctxt.instrumentSetIndex);
                                        uiRef.ctxt.setNameCharIndex = 0;
                                    },
                                    [&uiRef]() {
                                        uiRef.ctxt.instrumentSetIndex = clampSetSelection(uiRef.ctxt, uiRef.ctxt.instrumentSetIndex - 1);
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().setActiveSetIndex(uiRef.ctxt.instrumentSetIndex);
                                        uiRef.ctxt.setEntryIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().clampEntryIndex(uiRef.ctxt.instrumentSetIndex, 0);
                                        uiRef.ctxt.setNameDraft = uiRef.ctxt.rm.getInstrumentSetLibrary().getSetName(uiRef.ctxt.instrumentSetIndex);
                                        uiRef.ctxt.setNameCharIndex = 0;
                                    },
                                    [&uiRef]() {
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().setActiveSetIndex(uiRef.ctxt.instrumentSetIndex);
                                        applySelectedKeyInstrument(uiRef.ctxt, true);
                                    }
                                }
                            },
                            {"InSet",
                            [&uiRef]() {
                                syncSetSelection(uiRef.ctxt);
                                if(!currentSetHasEntries(uiRef.ctxt)){
                                    return std::string("-");
                                }
                                return getCurrentSetKeyDisplayName(uiRef.ctxt);
                            },
                                {
                                    [&uiRef]() {
                                        if(currentSetHasEntries(uiRef.ctxt)){
                                            stepSelectedKeyInstrument(uiRef.ctxt, +1);
                                        }
                                    },
                                    [&uiRef]() {
                                        if(currentSetHasEntries(uiRef.ctxt)){
                                            stepSelectedKeyInstrument(uiRef.ctxt, -1);
                                        }
                                    },
                                    [&uiRef]() {
                                        if(currentSetHasEntries(uiRef.ctxt)){
                                            applySelectedKeyInstrument(uiRef.ctxt, true);
                                        }
                                    }
                                }
                            },
                            {"I",
                            [&uiRef]() {
                                InstrumentCatalog& catalog = uiRef.ctxt.rm.getInstrumentCatalog();
                                const int count = catalog.size();
                                if(count <= 0){
                                    return std::string("-");
                                }
                                int index = uiRef.ctxt.setCatalogBrowseIndex % count;
                                if(index < 0) index += count;
                                uiRef.ctxt.setCatalogBrowseIndex = index;
                                return catalog.getDisplayName(static_cast<size_t>(index));
                            },
                                {
                                    [&uiRef]() {
                                        InstrumentCatalog& catalog = uiRef.ctxt.rm.getInstrumentCatalog();
                                        const int count = catalog.size();
                                        if(count <= 0){
                                            return;
                                        }
                                        int index = (uiRef.ctxt.setCatalogBrowseIndex + 1) % count;
                                        uiRef.ctxt.setCatalogBrowseIndex = index;
                                    },
                                    [&uiRef]() {
                                        InstrumentCatalog& catalog = uiRef.ctxt.rm.getInstrumentCatalog();
                                        const int count = catalog.size();
                                        if(count <= 0){
                                            return;
                                        }
                                        int index = (uiRef.ctxt.setCatalogBrowseIndex - 1 + count) % count;
                                        uiRef.ctxt.setCatalogBrowseIndex = index;
                                    },
                                    [&uiRef]() {
                                        if(uiRef.ctxt.rm.getInstrumentCatalog().size() <= 0){
                                            return;
                                        }
                                        uiRef.ctxt.instrumentIndex = uiRef.ctxt.setCatalogBrowseIndex;
                                        applyCatalogDefaults(uiRef.ctxt, true, true);
                                    }
                                }
                            },
                            {"Alias",
                            [&uiRef]() { return withCursorIndicator(uiRef.ctxt.setAliasDraft, uiRef.ctxt.setAliasCharIndex); },
                                {
                                    [&uiRef]() { cycleTextChar(uiRef.ctxt.setAliasDraft, uiRef.ctxt.setAliasCharIndex, +1); },
                                    [&uiRef]() { cycleTextChar(uiRef.ctxt.setAliasDraft, uiRef.ctxt.setAliasCharIndex, -1); },
                                    [&uiRef]() { uiRef.ctxt.setAliasDraft = trimTokenCopy(uiRef.ctxt.setAliasDraft); }
                                }
                            },
                            {"Apos",
                            [&uiRef]() {
                                ensureCursorAndText(uiRef.ctxt.setAliasDraft, uiRef.ctxt.setAliasCharIndex);
                                return std::to_string(uiRef.ctxt.setAliasCharIndex + 1) + "/" + std::to_string(uiRef.ctxt.setAliasDraft.size());
                            },
                                {
                                    [&uiRef]() { moveTextCursor(uiRef.ctxt.setAliasDraft, uiRef.ctxt.setAliasCharIndex, +1); },
                                    [&uiRef]() { moveTextCursor(uiRef.ctxt.setAliasDraft, uiRef.ctxt.setAliasCharIndex, -1); },
                                    []() {}
                                }
                            },
                            {"Add",
                            []() { return std::string("push"); },
                                {
                                    []() {},
                                    []() {},
                                    [&uiRef]() {
                                        syncSetSelection(uiRef.ctxt);
                                        InstrumentCatalog& catalog = uiRef.ctxt.rm.getInstrumentCatalog();
                                        const int count = catalog.size();
                                        if(count <= 0){
                                            return;
                                        }
                                        int index = uiRef.ctxt.setCatalogBrowseIndex % count;
                                        if(index < 0) index += count;
                                        uiRef.ctxt.setCatalogBrowseIndex = index;
                                        std::string alias = trimTokenCopy(uiRef.ctxt.setAliasDraft);
                                        if(alias.empty()){
                                            alias = catalog.getDisplayName(static_cast<size_t>(index));
                                            uiRef.ctxt.setAliasDraft = alias;
                                        }
                                        const std::string instrumentId = catalog.getFolderName(static_cast<size_t>(index));
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().addOrUpdateEntry(
                                            uiRef.ctxt.instrumentSetIndex,
                                            instrumentId,
                                            alias);
                                        const int entryCount = uiRef.ctxt.rm.getInstrumentSetLibrary().getEntryCount(uiRef.ctxt.instrumentSetIndex);
                                        for(int i = 0; i < entryCount; ++i){
                                            if(uiRef.ctxt.rm.getInstrumentSetLibrary().getEntry(uiRef.ctxt.instrumentSetIndex, i).instrumentId == instrumentId){
                                                uiRef.ctxt.setEntryIndex = i;
                                                break;
                                            }
                                        }
                                    }
                                }
                            },
                            {"Rem",
                            []() { return std::string("push"); },
                                {
                                    []() {},
                                    []() {},
                                    [&uiRef]() {
                                        syncSetSelection(uiRef.ctxt);
                                        if(!currentSetHasEntries(uiRef.ctxt)){
                                            return;
                                        }
                                        const InstrumentSetEntry entry = uiRef.ctxt.rm.getInstrumentSetLibrary().getEntry(
                                            uiRef.ctxt.instrumentSetIndex,
                                            uiRef.ctxt.setEntryIndex);
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().removeEntryByInstrumentId(
                                            uiRef.ctxt.instrumentSetIndex,
                                            entry.instrumentId);
                                        uiRef.ctxt.setEntryIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().clampEntryIndex(
                                            uiRef.ctxt.instrumentSetIndex,
                                            uiRef.ctxt.setEntryIndex);
                                    }
                                }
                            },
                            {"SName",
                            [&uiRef]() { return withCursorIndicator(uiRef.ctxt.setNameDraft, uiRef.ctxt.setNameCharIndex); },
                                {
                                    [&uiRef]() { cycleTextChar(uiRef.ctxt.setNameDraft, uiRef.ctxt.setNameCharIndex, +1); },
                                    [&uiRef]() { cycleTextChar(uiRef.ctxt.setNameDraft, uiRef.ctxt.setNameCharIndex, -1); },
                                    [&uiRef]() {
                                        std::string trimmed = trimTokenCopy(uiRef.ctxt.setNameDraft);
                                        if(trimmed.empty()){
                                            trimmed = "Set";
                                        }
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().renameSet(uiRef.ctxt.instrumentSetIndex, trimmed);
                                        uiRef.ctxt.setNameDraft = uiRef.ctxt.rm.getInstrumentSetLibrary().getSetName(uiRef.ctxt.instrumentSetIndex);
                                    }
                                }
                            },
                            {"SPos",
                            [&uiRef]() {
                                ensureCursorAndText(uiRef.ctxt.setNameDraft, uiRef.ctxt.setNameCharIndex);
                                return std::to_string(uiRef.ctxt.setNameCharIndex + 1) + "/" + std::to_string(uiRef.ctxt.setNameDraft.size());
                            },
                                {
                                    [&uiRef]() { moveTextCursor(uiRef.ctxt.setNameDraft, uiRef.ctxt.setNameCharIndex, +1); },
                                    [&uiRef]() { moveTextCursor(uiRef.ctxt.setNameDraft, uiRef.ctxt.setNameCharIndex, -1); },
                                    []() {}
                                }
                            },
                            {"NewSet",
                            []() { return std::string("push"); },
                                {
                                    []() {},
                                    []() {},
                                    [&uiRef]() {
                                        std::string trimmed = trimTokenCopy(uiRef.ctxt.setNameDraft);
                                        if(trimmed.empty()){
                                            trimmed = "Set";
                                        }
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().addSet(trimmed);
                                        uiRef.ctxt.instrumentSetIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().getActiveSetIndex();
                                        uiRef.ctxt.setEntryIndex = 0;
                                        uiRef.ctxt.setNameDraft = uiRef.ctxt.rm.getInstrumentSetLibrary().getSetName(uiRef.ctxt.instrumentSetIndex);
                                        uiRef.ctxt.setNameCharIndex = 0;
                                    }
                                }
                            },
                            {"DelSet",
                            []() { return std::string("push"); },
                                {
                                    []() {},
                                    []() {},
                                    [&uiRef]() {
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().removeSet(uiRef.ctxt.instrumentSetIndex);
                                        uiRef.ctxt.instrumentSetIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().getActiveSetIndex();
                                        uiRef.ctxt.setEntryIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().clampEntryIndex(uiRef.ctxt.instrumentSetIndex, 0);
                                        uiRef.ctxt.setNameDraft = uiRef.ctxt.rm.getInstrumentSetLibrary().getSetName(uiRef.ctxt.instrumentSetIndex);
                                        uiRef.ctxt.setNameCharIndex = 0;
                                    }
                                }
                            },
                            {"Use",
                            [&uiRef]() {
                                const int active = uiRef.ctxt.rm.getInstrumentSetLibrary().getActiveSetIndex();
                                return (active == uiRef.ctxt.instrumentSetIndex) ? std::string("Active") : std::string("push");
                            },
                                {
                                    []() {},
                                    []() {},
                                    [&uiRef]() {
                                        uiRef.ctxt.rm.getInstrumentSetLibrary().setActiveSetIndex(uiRef.ctxt.instrumentSetIndex);
                                        applySelectedKeyInstrument(uiRef.ctxt, true);
                                    }
                                }
                            }});
        return;
    }
    ///////--------------METRONOME PARAMS ------------------------------
    paramLines.push_back({"Metronome",  //Label
                        []{return "";}, //Value
                                {       //Manipulators
                                    []() {},
                                    []() {},
                                    []() {}
                                }
                        });
    subParamLines.push_back({ //Vector of subParamLines
                        {"MainOut",
                        [&uiRef]() { return uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Main1) ? "On" : "Off"; },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::Metronome, Output::Main1, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Main1));
                                             uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::Metronome, Output::Main2, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Main2)); },
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::Metronome, Output::Main1, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Main1));
                                             uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::Metronome, Output::Main2, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Main2)); },
                                []() {}
                            }
                        },
                        {"BPM",
                        [&uiRef]() { return std::to_string(static_cast<int>(uiRef.ctxt.metronome.getBPM())); },
                            {
                                [&uiRef]() { uiRef.ctxt.metronome.setBPM(uiRef.ctxt.metronome.getBPM() + 1); },
                                [&uiRef]() { uiRef.ctxt.metronome.setBPM(uiRef.ctxt.metronome.getBPM() - 1); },
                                []() {}
                            }
                        },
                        {"bp bar",
                        [&uiRef]() { return std::to_string(uiRef.ctxt.metronome.getBeatsPerBar()); },
                            {
                                [&uiRef]() {uiRef.ctxt.metronome.setBeatsPerBar(uiRef.ctxt.metronome.getBeatsPerBar() + 1); },
                                [&uiRef]() { uiRef.ctxt.metronome.setBeatsPerBar(uiRef.ctxt.metronome.getBeatsPerBar() - 1); },
                                []() {}
                            }
                        },
                        {"beatUnit",
                        [&uiRef]() { return std::to_string(uiRef.ctxt.metronome.getBeatUnit()); },
                            {
                                [&uiRef]() {uiRef.ctxt.metronome.setBeatUnit(uiRef.ctxt.metronome.getBeatUnit() + 1); },
                                [&uiRef]() { uiRef.ctxt.metronome.setBeatUnit(uiRef.ctxt.metronome.getBeatUnit() - 1); },
                                []() {}
                            }
                        }});
    ///////--------------INSTRUMENT PARAMS ------------------------------
    paramLines.push_back({"Instrument",  //Label
                        []{return "";}, //Value
                                {       //Manipulators
                                    []() {},
                                    []() {},
                                    []() {}
                                }
                        });
    subParamLines.push_back({ //Vector of subParamLines
                        {"Set",
                        [&uiRef]() {
                            syncSetSelection(uiRef.ctxt);
                            return uiRef.ctxt.rm.getInstrumentSetLibrary().getSetName(uiRef.ctxt.instrumentSetIndex);
                        },
                            {
                                [&uiRef]() {
                                    uiRef.ctxt.instrumentSetIndex = clampSetSelection(uiRef.ctxt, uiRef.ctxt.instrumentSetIndex + 1);
                                    uiRef.ctxt.setEntryIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().clampEntryIndex(uiRef.ctxt.instrumentSetIndex, 0);
                                    syncSetSelection(uiRef.ctxt);
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.instrumentSetIndex = clampSetSelection(uiRef.ctxt, uiRef.ctxt.instrumentSetIndex - 1);
                                    uiRef.ctxt.setEntryIndex = uiRef.ctxt.rm.getInstrumentSetLibrary().clampEntryIndex(uiRef.ctxt.instrumentSetIndex, 0);
                                    syncSetSelection(uiRef.ctxt);
                                },
                                [&uiRef]() {
                                    syncSetSelection(uiRef.ctxt);
                                    applySelectedKeyInstrument(uiRef.ctxt, true);
                                }
                            }
                        },
                        {"K",
                        [&uiRef]() { return getCurrentSetKeyDisplayName(uiRef.ctxt); },
                            {
                                [&uiRef]() {
                                    stepSelectedKeyInstrument(uiRef.ctxt, +1);
                                },
                                [&uiRef]() {
                                    stepSelectedKeyInstrument(uiRef.ctxt, -1);
                                },
                                [&uiRef]() {
                                    applySelectedKeyInstrument(uiRef.ctxt, true);
                                }
                            }
                        },
                        {"D",
                        [&uiRef]() { return uiRef.ctxt.rm.getDrumCatalog().getDisplayName(uiRef.ctxt.drumIndex); },
                            {
                                [&uiRef]() {
                                    uiRef.ctxt.drumIndex = (uiRef.ctxt.drumIndex + 1) % uiRef.ctxt.rm.getDrumCatalog().size();
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.drumIndex = (uiRef.ctxt.drumIndex - 1 + uiRef.ctxt.rm.getDrumCatalog().size()) % uiRef.ctxt.rm.getDrumCatalog().size();
                                },
                                [&uiRef]() {
                                    applyCatalogDefaults(uiRef.ctxt, false, true);
                                }
                            }
                        },
                        {"Active",
                        [&uiRef]() { return uiRef.ctxt.rm.keysInMelodicMode ? "Melodic" : "Drums"; },
                            {
                                [&uiRef]() {
                                    uiRef.ctxt.rm.keysInMelodicMode = !uiRef.ctxt.rm.keysInMelodicMode;
                                    if(uiRef.ctxt.rm.keysInMelodicMode){
                                        applySelectedKeyInstrument(uiRef.ctxt, false);
                                    } else {
                                        applyCatalogDefaults(uiRef.ctxt, false, false);
                                    }
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.rm.keysInMelodicMode = !uiRef.ctxt.rm.keysInMelodicMode;
                                    if(uiRef.ctxt.rm.keysInMelodicMode){
                                        applySelectedKeyInstrument(uiRef.ctxt, false);
                                    } else {
                                        applyCatalogDefaults(uiRef.ctxt, false, false);
                                    }
                                },
                                []() {}
                            }
                        },
                        {"Attack",
                        [&uiRef]() {
                            return std::to_string(getActiveModeSamplePack(uiRef.ctxt).getVoiceSettings().attack);
                        },
                            {
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.attack = clampFx(settings.attack + 0.005f, 0.0f, 5.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.attack = clampFx(settings.attack - 0.005f, 0.0f, 5.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                []() {}
                            }
                        },
                        {"Decay",
                        [&uiRef]() {
                            return std::to_string(getActiveModeSamplePack(uiRef.ctxt).getVoiceSettings().decay);
                        },
                            {
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.decay = clampFx(settings.decay + 0.005f, 0.0f, 5.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.decay = clampFx(settings.decay - 0.005f, 0.0f, 5.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                []() {}
                            }
                        },
                        {"Sustain",
                        [&uiRef]() {
                            return std::to_string(getActiveModeSamplePack(uiRef.ctxt).getVoiceSettings().sustain);
                        },
                            {
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.sustain = clampFx(settings.sustain + 0.05f, 0.0f, 1.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.sustain = clampFx(settings.sustain - 0.05f, 0.0f, 1.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                []() {}
                            }
                        },
                        {"Release",
                        [&uiRef]() {
                            return std::to_string(getActiveModeSamplePack(uiRef.ctxt).getVoiceSettings().release);
                        },
                            {
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.release = clampFx(settings.release + 0.005f, 0.0f, 5.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.release = clampFx(settings.release - 0.005f, 0.0f, 5.0f);
                                    samplePack.setVoiceSettings(settings);
                                },
                                []() {}
                            }
                        },
                        {"Repeat",
                        [&uiRef]() {
                            return getActiveModeSamplePack(uiRef.ctxt).getVoiceSettings().repeat ? "On" : "Off";
                        },
                            {
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.repeat = !settings.repeat;
                                    samplePack.setVoiceSettings(settings);
                                },
                                [&uiRef]() {
                                    SamplePack& samplePack = getActiveModeSamplePack(uiRef.ctxt);
                                    SamplePackVoiceSettings settings = samplePack.getVoiceSettings();
                                    settings.repeat = !settings.repeat;
                                    samplePack.setVoiceSettings(settings);
                                },
                                []() {}
                            }
                        }});
    ///////--------------LOOPER PARAMS ------------------------------
    paramLines.push_back({"Looper",  //Label
                        []{return "";}, //Value
                                {       //Manipulators
                                    []() {},
                                    []() {},
                                    []() {}
                                }
                        });
    subParamLines.push_back({ //Vector of subParamLines
                        {"pos",
                        [&uiRef]() {if(uiRef.ctxt.loopers.getLooperTriggerMode() != LooperTriggerMode::OnBar){
                                        std::string res = "";
                                        int prog = (int)(uiRef.ctxt.loopers.getProgress()*10);
                                        for(int i = 0; i < prog; i++){
                                            res += "|";
                                        }
                                        return res;
                                    }
                                    int totalBars = uiRef.ctxt.loopers.getLengthInBars();
                                    int barsCompleted = uiRef.ctxt.loopers.getProgressInBars();
                                    return std::to_string(barsCompleted + 1) + " / " + std::to_string(totalBars); },//+1 for not zero based
                            {
                                []() {},
                                []() {},
                                []() {}
                            }
                        },
                        {"Trigger",
                        [&uiRef]() { return getLooperTriggerModeString(uiRef.ctxt.loopers.getLooperTriggerMode()); },
                            {
                                [&uiRef]() { LooperTriggerMode mode = uiRef.ctxt.loopers.getLooperTriggerMode();
                                            mode = (LooperTriggerMode)(((int)mode + 1) % (int)LooperTriggerMode::COUNT);
                                            uiRef.ctxt.loopers.setLooperTriggerMode(mode);},
                                [&uiRef]() { LooperTriggerMode mode = uiRef.ctxt.loopers.getLooperTriggerMode();
                                            mode = (LooperTriggerMode)(((int)mode - 1) % (int)LooperTriggerMode::COUNT);
                                            uiRef.ctxt.loopers.setLooperTriggerMode(mode);},
                                []() {}
                            }
                        },
                        {"LooperId",
                        [&uiRef]() {return std::to_string(uiRef.ctxt.loopers.getEditableLooperIndex());},
                            {
                                [&uiRef]() { uiRef.ctxt.loopers.setEditableLooperIndex((uiRef.ctxt.loopers.getEditableLooperIndex() + 1) % uiRef.ctxt.loopers.size());},
                                [&uiRef]() { int Id = uiRef.ctxt.loopers.getEditableLooperIndex() - 1;
                                             if(Id < 0) Id = uiRef.ctxt.loopers.size() - 1;
                                             uiRef.ctxt.loopers.setEditableLooperIndex(Id % uiRef.ctxt.loopers.size());},
                                []() {}
                            }
                        },
                        {"#Bars",
                        [&uiRef]() {Loopers& loopers = uiRef.ctxt.loopers;
                                    std::string displayValue;
                                    if(loopers.isEmpty(loopers.getEditableLooperIndex()))
                                        displayValue = "set: " + std::to_string(uiRef.ctxt.numberOfBarsForLoopers);
                                    else
                                        displayValue = "fix: " + std::to_string(uiRef.ctxt.loopers.getLengthInBars());
                                    return displayValue;},
                            {
                                [&uiRef]() { uiRef.ctxt.numberOfBarsForLoopers += 1; },
                                [&uiRef]() { uiRef.ctxt.numberOfBarsForLoopers -= 1; },
                                [&uiRef]() { uiRef.ctxt.loopers.setLengthInBars(uiRef.ctxt.numberOfBarsForLoopers);}
                            }
                        }});
    ///////--------------ROUTER PARAMS ------------------------------
    paramLines.push_back({"Router",  //Label
                        []{return "";}, //Value
                                {       //Manipulators
                                    []() {},
                                    []() {},
                                    []() {}
                                }
                        });
    subParamLines.push_back({ //Vector of subParamLines
                        {"In1->Main1",
                        [&uiRef]() { return uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::AudioInL, Output::Main1) ? "On" : "Off"; },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::AudioInL, Output::Main1, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::AudioInL, Output::Main1)); },
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::AudioInL, Output::Main1, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::AudioInL, Output::Main1)); },
                                []() {}
                            }
                        },
                        {"In2->Main2",
                        [&uiRef]() { return uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::AudioInR, Output::Main2) ? "On" : "Off"; },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::AudioInR, Output::Main2, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::AudioInR, Output::Main2)); },
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::AudioInR, Output::Main2, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::AudioInR, Output::Main2)); },
                                []() {}
                            }
                        },
                        {"In1->Lpr",
                        [&uiRef]() { return uiRef.ctxt.rm.getSignalRouter().getPath(Signal::AudioInL, Signal::Loopers) ? "On" : "Off"; },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setPath(Signal::AudioInL, Signal::Loopers, !uiRef.ctxt.rm.getSignalRouter().getPath(Signal::AudioInL, Signal::Loopers)); },
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setPath(Signal::AudioInL, Signal::Loopers, !uiRef.ctxt.rm.getSignalRouter().getPath(Signal::AudioInL, Signal::Loopers)); },
                                []() {}
                            }
                        },
                        {"In2->Lpr",
                        [&uiRef]() { return uiRef.ctxt.rm.getSignalRouter().getPath(Signal::AudioInR, Signal::Loopers) ? "On" : "Off"; },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setPath(Signal::AudioInR, Signal::Loopers, !uiRef.ctxt.rm.getSignalRouter().getPath(Signal::AudioInR, Signal::Loopers)); },
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setPath(Signal::AudioInR, Signal::Loopers, !uiRef.ctxt.rm.getSignalRouter().getPath(Signal::AudioInR, Signal::Loopers)); },
                                []() {}
                            }
                        }});
    //////--------------EFFECTS PARAMS ------------------------------
    paramLines.push_back({"Effects",
                        []{return "";},
                                {
                                    []() {},
                                    []() {},
                                    []() {}
                                }
                            });
    subParamLines.push_back({
                        {"Target",
                        [&uiRef]() {
                            clampEffectsSelection(uiRef.ctxt);
                            return getEffectsTargetName(uiRef.ctxt, uiRef.ctxt.effectsTargetIndex);
                        },
                            {
                                [&uiRef]() { uiRef.ctxt.effectsTargetIndex += 1; clampEffectsSelection(uiRef.ctxt); },
                                [&uiRef]() { uiRef.ctxt.effectsTargetIndex -= 1; clampEffectsSelection(uiRef.ctxt); },
                                []() {}
                            }
                        },
                        {"Stage",
                        [&uiRef]() {
                            clampEffectsSelection(uiRef.ctxt);
                            std::vector<EffectDescriptor> effects = getDisplayEffectsChain(uiRef.ctxt).getEffects();
                            if(effects.empty()) return std::string("None");
                            return std::to_string(uiRef.ctxt.effectsStageIndex + 1) + "/" + std::to_string(effects.size());
                        },
                            {
                                [&uiRef]() { uiRef.ctxt.effectsStageIndex += 1; clampEffectsSelection(uiRef.ctxt); },
                                [&uiRef]() { uiRef.ctxt.effectsStageIndex -= 1; clampEffectsSelection(uiRef.ctxt); },
                                []() {}
                            }
                        },
                        {"NewType",
                        [&uiRef]() {
                            clampEffectsSelection(uiRef.ctxt);
                            return effectTypeToString(effectTypeFromIndex(uiRef.ctxt.effectsNewTypeIndex));
                        },
                            {
                                [&uiRef]() { uiRef.ctxt.effectsNewTypeIndex += 1; clampEffectsSelection(uiRef.ctxt); },
                                [&uiRef]() { uiRef.ctxt.effectsNewTypeIndex -= 1; clampEffectsSelection(uiRef.ctxt); },
                                []() {}
                            }
                        },
                        {"Add",
                        []() { return std::string("push"); },
                            {
                                []() {},
                                []() {},
                                [&uiRef]() { addEffectOnCurrentTarget(uiRef.ctxt); }
                            }
                        },
                        {"Remove",
                        [&uiRef]() {
                            EffectDescriptor desc;
                            if(!getSelectedEffectDescriptor(uiRef.ctxt, desc)) return std::string("-");
                            return effectTypeToString(desc.type);
                        },
                            {
                                []() {},
                                []() {},
                                [&uiRef]() { removeSelectedEffectOnCurrentTarget(uiRef.ctxt); }
                            }
                        },
                        {"Enabled",
                        [&uiRef]() {
                            EffectDescriptor desc;
                            if(!getSelectedEffectDescriptor(uiRef.ctxt, desc)) return std::string("-");
                            return desc.params.enabled ? std::string("On") : std::string("Off");
                        },
                            {
                                [&uiRef]() { applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType){ p.enabled = !p.enabled; }); },
                                [&uiRef]() { applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType){ p.enabled = !p.enabled; }); },
                                []() {}
                            }
                        },
                        {"P1",
                        [&uiRef]() { return getSelectedEffectParamDisplay(uiRef.ctxt, 0); },
                            {
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 0, +1); },
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 0, -1); },
                                []() {}
                            }
                        },
                        {"P2",
                        [&uiRef]() { return getSelectedEffectParamDisplay(uiRef.ctxt, 1); },
                            {
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 1, +1); },
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 1, -1); },
                                []() {}
                            }
                        },
                        {"P3",
                        [&uiRef]() { return getSelectedEffectParamDisplay(uiRef.ctxt, 2); },
                            {
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 2, +1); },
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 2, -1); },
                                []() {}
                            }
                        },
                        {"P4",
                        [&uiRef]() { return getSelectedEffectParamDisplay(uiRef.ctxt, 3); },
                            {
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 3, +1); },
                                [&uiRef]() { adjustSelectedEffectParam(uiRef.ctxt, 3, -1); },
                                []() {}
                            }
                        }});
    //////--------------SAMPLER PARAMS ------------------------------
    paramLines.push_back({"Sampler",
                        []{return "";},
                                {
                                    []() {},
                                    []() {},
                                    []() {}
                                }
                            });
    subParamLines.push_back({
                        {"Sample",
                        [&uiRef]() {
                            Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                            uiRef.ctxt.samplerIndex = samplers.clampSamplerSelection(uiRef.ctxt.samplerIndex);
                            int sampleIndex = uiRef.ctxt.samplerIndex;
                            if(sampleIndex >= samplers.getNumSamples()){
                                return std::string("New");
                            }
                            if(!samplers.hasSampleAudio(sampleIndex)){
                                return std::string("Empty");
                            }
                            return std::to_string(sampleIndex + 1) + "/" + std::to_string(samplers.getNumSamples());
                        },
                            {
                                [&uiRef]() {
                                    Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                                    uiRef.ctxt.samplerIndex = samplers.clampSamplerSelection(uiRef.ctxt.samplerIndex + 1);
                                    uiRef.ctxt.sliceIndex = samplers.clampSliceSelection(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex);
                                },
                                [&uiRef]() {
                                    Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                                    uiRef.ctxt.samplerIndex = samplers.clampSamplerSelection(uiRef.ctxt.samplerIndex - 1);
                                    uiRef.ctxt.sliceIndex = samplers.clampSliceSelection(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex);
                                },
                                []() {}
                            }
                        },
                        {"Record",
                        [&uiRef]() {
                            Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                            if(samplers.isRecording(uiRef.ctxt.samplerIndex)) return std::string("Rec");
                            if(samplers.isRecording()) return std::string("Busy");
                            return std::string("Idle");
                        },
                            {
                                []() {},
                                []() {},
                                [&uiRef]() { uiRef.samplerToggleRecord(); }
                            }
                        },
                        {"AutoN",
                        [&uiRef]() { return std::to_string(uiRef.ctxt.numberOfSliceForAutoSlice); },
                            {
                                [&uiRef]() { uiRef.ctxt.numberOfSliceForAutoSlice = std::min(32, uiRef.ctxt.numberOfSliceForAutoSlice + 1); },
                                [&uiRef]() { uiRef.ctxt.numberOfSliceForAutoSlice = std::max(1, uiRef.ctxt.numberOfSliceForAutoSlice - 1); },
                                []() {}
                            }
                        },
                        {"Auto",
                        []() { return std::string("push"); },
                            {
                                []() {},
                                []() {},
                                [&uiRef]() {
                                    Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                                    samplers.autoSlice(uiRef.ctxt.samplerIndex, uiRef.ctxt.numberOfSliceForAutoSlice);
                                    uiRef.ctxt.sliceIndex = samplers.clampSliceSelection(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex);
                                }
                            }
                        },
                        {"Slice",
                        [&uiRef]() {
                            Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                            int numSlices = samplers.getNumSlices(uiRef.ctxt.samplerIndex);
                            if(numSlices <= 0) return std::string("-");
                            uiRef.ctxt.sliceIndex = samplers.clampSliceSelection(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex);
                            return std::to_string(uiRef.ctxt.sliceIndex + 1) + "/" + std::to_string(numSlices);
                        },
                            {
                                [&uiRef]() {
                                    Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                                    if(samplers.getNumSlices(uiRef.ctxt.samplerIndex) > 0){
                                        uiRef.ctxt.sliceIndex = samplers.clampSliceSelection(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex + 1);
                                    }
                                },
                                [&uiRef]() {
                                    Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                                    if(samplers.getNumSlices(uiRef.ctxt.samplerIndex) > 0){
                                        uiRef.ctxt.sliceIndex = samplers.clampSliceSelection(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex - 1);
                                    }
                                },
                                []() {}
                            }
                        },
                        {"Pitch",
                        [&uiRef]() {
                            Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                            if(samplers.getNumSlices(uiRef.ctxt.samplerIndex) <= 0) return std::string("-");
                            float pitch = samplers.getSlicePitchSemitones(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex);
                            return std::to_string(pitch);
                        },
                            {
                                [&uiRef]() {
                                    uiRef.ctxt.rm.getSamplers().adjustSlicePitchSemitones(
                                        uiRef.ctxt.samplerIndex,
                                        uiRef.ctxt.sliceIndex,
                                        0.5f);
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.rm.getSamplers().adjustSlicePitchSemitones(
                                        uiRef.ctxt.samplerIndex,
                                        uiRef.ctxt.sliceIndex,
                                        -0.5f);
                                },
                                []() {}
                            }
                        },
                        {"KeyMode",
                        [&uiRef]() { return uiRef.ctxt.rm.getSamplers().getKeyboardModeName(); },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.getSamplers().cycleKeyboardMode(1); },
                                [&uiRef]() { uiRef.ctxt.rm.getSamplers().cycleKeyboardMode(-1); },
                                []() {}
                            }
                        },
                        {"Manual",
                        [&uiRef]() {
                            Samplers& samplers = uiRef.ctxt.rm.getSamplers();
                            if(samplers.getNumSlices(uiRef.ctxt.samplerIndex) <= 0) return std::string("-");
                            int start = static_cast<int>(100.0f * samplers.getSliceStartNormalized(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex));
                            int end = static_cast<int>(100.0f * samplers.getSliceEndNormalized(uiRef.ctxt.samplerIndex, uiRef.ctxt.sliceIndex));
                            std::string boundary = uiRef.ctxt.editSliceStartBoundary ? "S" : "E";
                            return boundary + ":" + std::to_string(start) + "-" + std::to_string(end);
                        },
                            {
                                []() {},
                                []() {},
                                []() {}
                            }
                        }});
    //////--------------RECORDER PARAMS ------------------------------
    paramLines.push_back({"Recorder",  //Label
                        []{return "";}, //Value
                                {       //Manipulators
                                    []() {},
                                    []() {},
                                    []() {}
                                }
                            });
    subParamLines.push_back({ //Vector of subParamLines
                        {"Record",
                        [&uiRef]() { return uiRef.ctxt.rm.getRecorder().isRecording() ? "On" : "Off"; },
                            {
                                []() {},
                                []() {},
                                [&uiRef]() { uiRef.ctxt.rm.getRecorder().isRecording() ? uiRef.ctxt.rm.getRecorder().stopRecording() : uiRef.ctxt.rm.getRecorder().startRecording();}
                            }
                        }});
}
