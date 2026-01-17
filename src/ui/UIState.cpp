 #include "../../include/ui/UI.h"
 #include "../../include/general/ResourceManager.h"
 #include <string>

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

 UIState::UIState(UI& uiRef, UIStateId stateId)
     : id(stateId)
{
    switch (id) {
        case UIStateId::General:
            paramLines = {
                {"Metronome", []{return "";}},
                {"Instrument", []{return "";}},
                {"Looper", []{return "";}}
            };
            subParamLines = {
                //Metronome submenu
                {
                    {"MainOut", [&uiRef]() { return uiRef.ctxt.metronome.mainOutIsOn() ? "On" : "Off"; }},
                    {"AnalogOut", [&uiRef]() { return uiRef.ctxt.metronome.analogOutIsOn() ? "On" : "Off"; }},
                    {"BPM", [&uiRef]() { return std::to_string(uiRef.ctxt.metronome.getBPM()); }},
                    {"bp bar", [&uiRef]() { return std::to_string(uiRef.ctxt.metronome.getBeatsPerBar()); }},
                    {"beatUnit", [&uiRef]() { return std::to_string(uiRef.ctxt.metronome.getBeatUnit()); }}
                },
                //Instrument submenu
                {
                    {"Keys", [&uiRef]() { return uiRef.ctxt.rm.getInstrumentCatalog().getDisplayName(uiRef.ctxt.instrumentIndex); }},
                    {"Drums", [&uiRef]() { return uiRef.ctxt.rm.getDrumCatalog().getDisplayName(uiRef.ctxt.drumIndex); }},
                    {"Active", [&uiRef]() { return uiRef.ctxt.rm.keysInMelodicMode ? "Melodic" : "Drums"; }}
                },
                //Looper submenu
                {
                    {"TriggerMode", [&uiRef]() { return getLooperTriggerModeString(uiRef.ctxt.loopers.getLooperTriggerMode()); }}
                }
            };
            manipulators = {
                {
                    []() {},
                    []() {},
                    []() {}
                },
                {
                    []() {},
                    []() {},
                    []() {}
                }
            };
            subParamManipulators = {
                {
                    //Metronome manipulators
                    { //Toggle Metronome
                        [&uiRef]() { uiRef.ctxt.metronome.mainOutonOffToggle(); },
                        [&uiRef]() { uiRef.ctxt.metronome.mainOutonOffToggle(); },
                        []() {}
                    },
                    { //Toggle Analog Out
                        [&uiRef]() { uiRef.ctxt.metronome.analogOutonOffToggle(); },
                        [&uiRef]() { uiRef.ctxt.metronome.analogOutonOffToggle(); },
                        []() {}
                    },
                    { //BPM
                        [&uiRef]() { uiRef.ctxt.metronome.setBPM(uiRef.ctxt.metronome.getBPM() + 1); },
                        [&uiRef]() { uiRef.ctxt.metronome.setBPM(uiRef.ctxt.metronome.getBPM() - 1); },
                        []() {}
                    },
                    
                    { // Beats per Bar
                        [&uiRef]() {uiRef.ctxt.metronome.setBeatsPerBar(uiRef.ctxt.metronome.getBeatsPerBar() + 1); },
                        [&uiRef]() { uiRef.ctxt.metronome.setBeatsPerBar(uiRef.ctxt.metronome.getBeatsPerBar() - 1); },
                        []() {}
                    },
                    { // Beat Unit
                        [&uiRef]() {uiRef.ctxt.metronome.setBeatUnit(uiRef.ctxt.metronome.getBeatUnit() + 1); },
                        [&uiRef]() { uiRef.ctxt.metronome.setBeatUnit(uiRef.ctxt.metronome.getBeatUnit() - 1); },
                        []() {}
                    }
                },
                //Instrument manipulators
                {
                    { // Change Instrument
                        [&uiRef]() {
                            uiRef.ctxt.instrumentIndex = (uiRef.ctxt.instrumentIndex + 1) % uiRef.ctxt.rm.getInstrumentCatalog().size();
                        },
                        [&uiRef]() {
                            uiRef.ctxt.instrumentIndex = (uiRef.ctxt.instrumentIndex - 1 + uiRef.ctxt.rm.getInstrumentCatalog().size()) % uiRef.ctxt.rm.getInstrumentCatalog().size();
                        },
                        [&uiRef]() {
                            uiRef.ctxt.rm.getKeyInstrumentSamplePack().initForFolder(
                                uiRef.ctxt.rm.getInstrumentCatalog().getFolderName(uiRef.ctxt.instrumentIndex)
                            );
                        }
                    },
                    { // Change Drums
                        [&uiRef]() {
                            uiRef.ctxt.drumIndex = (uiRef.ctxt.drumIndex + 1) % uiRef.ctxt.rm.getDrumCatalog().size();
                        },
                        [&uiRef]() {
                            uiRef.ctxt.drumIndex = (uiRef.ctxt.drumIndex - 1 + uiRef.ctxt.rm.getDrumCatalog().size()) % uiRef.ctxt.rm.getDrumCatalog().size();
                        },
                        [&uiRef]() {
                            uiRef.ctxt.rm.getDrumSamplePack().initForFolder(
                                uiRef.ctxt.rm.getDrumCatalog().getFolderName(uiRef.ctxt.drumIndex)
                            );
                        }
                    },
                    { //Toggle Instrument Drum/Keys
                        [&uiRef]() { uiRef.ctxt.rm.keysInMelodicMode = !uiRef.ctxt.rm.keysInMelodicMode; },
                        [&uiRef]() { uiRef.ctxt.rm.keysInMelodicMode = !uiRef.ctxt.rm.keysInMelodicMode; },
                        []() {}
                    }
                },
                //Looper manipulators
                {
                    { // Toggle Trigger Mode
                        [&uiRef]() { uiRef.ctxt.loopers.looperTriggerModeToggle(); },
                        [&uiRef]() { uiRef.ctxt.loopers.looperTriggerModeToggle(); },
                        []() {}
                    }
                }
            };
            break;
        // Additional states can be initialized here
        default:
            paramLines = {
                {"Wrong Menu", []{return "";}}
            };
            subParamLines = {
                {
                    {"No", []() { return ""; }},
                    {"Submenu", []() { return ""; }}
                }
            };
            manipulators = {
                {
                    []() {},
                    []() {},
                    []() {}
                }
            };
            subParamManipulators = {
                {
                    {
                        []() {},
                        []() {},
                        []() {}
                    },
                    {
                        []() {},
                        []() {},
                        []() {}
                    }
                }
            };
            break;
    }
}
