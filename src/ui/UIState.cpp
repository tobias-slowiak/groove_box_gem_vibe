 #include "../../include/ui/UI.h"
 #include "../../include/general/ResourceManager.h"
 #include "../../include/audio/Loopers.h"
#include "../../include/audio/Effects.h"
 #include <string>
 #include <algorithm>
 #include <functional>
 //compile

UIStateContext::UIStateContext(ResourceManager& rm): rm(rm), metronome(rm.getMetronome()), loopers(rm.getLoopers()) {}

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
        case 2: return EffectType::Delay;
        case 3: return EffectType::Reverb;
        default: return EffectType::LowPass;
    }
}

std::string effectTypeToString(EffectType type){
    switch(type){
        case EffectType::LowPass: return "LP";
        case EffectType::HighPass: return "HP";
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

    int effectTypeCount = 4;
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




 UIState::UIState(UI& uiRef, UIStateId stateId)
     : id(stateId)
{
    paramLines.clear();
    subParamLines.clear();
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
                        [&uiRef]() { return std::to_string(uiRef.ctxt.metronome.getBPM()); },
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
                        {"Keys",
                        [&uiRef]() { return uiRef.ctxt.rm.getInstrumentCatalog().getDisplayName(uiRef.ctxt.instrumentIndex); },
                            {
                                [&uiRef]() {
                                    uiRef.ctxt.instrumentIndex = (uiRef.ctxt.instrumentIndex + 1) % uiRef.ctxt.rm.getInstrumentCatalog().size();
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.instrumentIndex = (uiRef.ctxt.instrumentIndex - 1 + uiRef.ctxt.rm.getInstrumentCatalog().size()) % uiRef.ctxt.rm.getInstrumentCatalog().size();
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.rm.getKeyInstrumentSamplePack().initForFolder(
                                        uiRef.ctxt.rm.getInstrumentCatalog().getFolderName(uiRef.ctxt.instrumentIndex));
                                }
                            }
                        },
                        {"Drums",
                        [&uiRef]() { return uiRef.ctxt.rm.getDrumCatalog().getDisplayName(uiRef.ctxt.drumIndex); },
                            {
                                [&uiRef]() {
                                    uiRef.ctxt.drumIndex = (uiRef.ctxt.drumIndex + 1) % uiRef.ctxt.rm.getDrumCatalog().size();
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.drumIndex = (uiRef.ctxt.drumIndex - 1 + uiRef.ctxt.rm.getDrumCatalog().size()) % uiRef.ctxt.rm.getDrumCatalog().size();
                                },
                                [&uiRef]() {
                                    uiRef.ctxt.rm.getDrumSamplePack().initForFolder(
                                        uiRef.ctxt.rm.getDrumCatalog().getFolderName(uiRef.ctxt.drumIndex));
                                }
                            }
                        },
                        {"Active",
                        [&uiRef]() { return uiRef.ctxt.rm.keysInMelodicMode ? "Melodic" : "Drums"; },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.keysInMelodicMode = !uiRef.ctxt.rm.keysInMelodicMode; },
                                [&uiRef]() { uiRef.ctxt.rm.keysInMelodicMode = !uiRef.ctxt.rm.keysInMelodicMode; },
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
                        {"Mix",
                        [&uiRef]() {
                            EffectDescriptor desc;
                            if(!getSelectedEffectDescriptor(uiRef.ctxt, desc)) return std::string("-");
                            return std::to_string(desc.params.mix);
                        },
                            {
                                [&uiRef]() { applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType){ p.mix = clampFx(p.mix + 0.05f, 0.0f, 1.0f); }); },
                                [&uiRef]() { applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType){ p.mix = clampFx(p.mix - 0.05f, 0.0f, 1.0f); }); },
                                []() {}
                            }
                        },
                        {"Param1",
                        [&uiRef]() {
                            EffectDescriptor desc;
                            if(!getSelectedEffectDescriptor(uiRef.ctxt, desc)) return std::string("-");
                            if(desc.type == EffectType::LowPass || desc.type == EffectType::HighPass){
                                return "cut:" + std::to_string(static_cast<int>(desc.params.cutoffHz));
                            }
                            if(desc.type == EffectType::Delay){
                                if(desc.params.delayTempoNote != DelayTempoNote::Off){
                                    const float syncedMs = delayTempoNoteToMs(desc.params.delayTempoNote, uiRef.ctxt.metronome.getBPM());
                                    return "ms:" + std::to_string(static_cast<int>(syncedMs));
                                }
                                return "ms:" + std::to_string(static_cast<int>(desc.params.delayMs));
                            }
                            return "room:" + std::to_string(desc.params.roomSize);
                        },
                            {
                                [&uiRef]() {
                                    applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType type){
                                        if(type == EffectType::LowPass || type == EffectType::HighPass){
                                            p.cutoffHz = clampFx(p.cutoffHz + 100.0f, 20.0f, 18000.0f);
                                        } else if(type == EffectType::Delay){
                                            p.delayMs = clampFx(p.delayMs + 10.0f, 1.0f, 2000.0f);
                                            p.delayTempoNote = DelayTempoNote::Off;
                                        } else {
                                            p.roomSize = clampFx(p.roomSize + 0.05f, 0.0f, 1.0f);
                                        }
                                    });
                                },
                                [&uiRef]() {
                                    applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType type){
                                        if(type == EffectType::LowPass || type == EffectType::HighPass){
                                            p.cutoffHz = clampFx(p.cutoffHz - 100.0f, 20.0f, 18000.0f);
                                        } else if(type == EffectType::Delay){
                                            p.delayMs = clampFx(p.delayMs - 10.0f, 1.0f, 2000.0f);
                                            p.delayTempoNote = DelayTempoNote::Off;
                                        } else {
                                            p.roomSize = clampFx(p.roomSize - 0.05f, 0.0f, 1.0f);
                                        }
                                    });
                                },
                                []() {}
                            }
                        },
                        {"Param2",
                        [&uiRef]() {
                            EffectDescriptor desc;
                            if(!getSelectedEffectDescriptor(uiRef.ctxt, desc)) return std::string("-");
                            if(desc.type == EffectType::Delay){
                                return "fb:" + std::to_string(desc.params.feedback);
                            }
                            if(desc.type == EffectType::Reverb){
                                return "damp:" + std::to_string(desc.params.damping);
                            }
                            return std::string("-");
                        },
                            {
                                [&uiRef]() {
                                    applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType type){
                                        if(type == EffectType::Delay){
                                            p.feedback = clampFx(p.feedback + 0.05f, 0.0f, 0.98f);
                                        } else if(type == EffectType::Reverb){
                                            p.damping = clampFx(p.damping + 0.05f, 0.0f, 1.0f);
                                        }
                                    });
                                },
                                [&uiRef]() {
                                    applyToSelectedEffect(uiRef.ctxt, [](EffectParameters& p, EffectType type){
                                        if(type == EffectType::Delay){
                                            p.feedback = clampFx(p.feedback - 0.05f, 0.0f, 0.98f);
                                        } else if(type == EffectType::Reverb){
                                            p.damping = clampFx(p.damping - 0.05f, 0.0f, 1.0f);
                                        }
                                    });
                                },
                                []() {}
                            }
                        },
                        {"DelayNote",
                        [&uiRef]() {
                            EffectDescriptor desc;
                            if(!getSelectedEffectDescriptor(uiRef.ctxt, desc)) return std::string("-");
                            if(desc.type != EffectType::Delay) return std::string("-");
                            return delayTempoNoteToString(desc.params.delayTempoNote);
                        },
                            {
                                [&uiRef]() {
                                    applyToSelectedEffect(uiRef.ctxt, [&uiRef](EffectParameters& p, EffectType type){
                                        if(type == EffectType::Delay){
                                            p.delayTempoNote = cycleDelayTempoNote(p.delayTempoNote, 1);
                                            if(p.delayTempoNote != DelayTempoNote::Off){
                                                p.delaySyncToTempo = false;
                                                p.delayMs = clampFx(delayTempoNoteToMs(p.delayTempoNote, uiRef.ctxt.metronome.getBPM()), 1.0f, 2000.0f);
                                            }
                                        }
                                    });
                                },
                                [&uiRef]() {
                                    applyToSelectedEffect(uiRef.ctxt, [&uiRef](EffectParameters& p, EffectType type){
                                        if(type == EffectType::Delay){
                                            p.delayTempoNote = cycleDelayTempoNote(p.delayTempoNote, -1);
                                            if(p.delayTempoNote != DelayTempoNote::Off){
                                                p.delaySyncToTempo = false;
                                                p.delayMs = clampFx(delayTempoNoteToMs(p.delayTempoNote, uiRef.ctxt.metronome.getBPM()), 1.0f, 2000.0f);
                                            }
                                        }
                                    });
                                },
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
