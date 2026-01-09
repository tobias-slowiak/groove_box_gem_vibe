 #include "../../include/ui/UI.h"
 #include "../../include/general/ResourceManager.h"
 #include <string>

 UIState::UIState(UI& uiRef, UIStateId stateId)
     : id(stateId)
{
    switch (id) {
        case UIStateId::General:
            paramLines = {
                {"Rythm", []{return "";}},
                {"Instrument", []{return "";}}
            };
            subParamLines = {
                {
                    {"BPM", [&uiRef]() { return std::to_string(uiRef.ctxt.bpm); }},
                    {"Metronome", [&uiRef]() { return uiRef.ctxt.metronomeOn ? "On" : "Off"; }}
                },
                {
                    {"Keys", [&uiRef]() { return uiRef.ctxt.rm.getInstrumentCatalog().getDisplayName(uiRef.ctxt.instrumentIndex); }},
                    {"Drums", [&uiRef]() { return uiRef.ctxt.rm.getDrumCatalog().getDisplayName(uiRef.ctxt.drumIndex); }},
                    {"Active", [&uiRef]() { return uiRef.ctxt.rm.keysInMelodicMode ? "Melodic" : "Drums"; }}
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
                    { //BPM
                        [&uiRef]() { uiRef.ctxt.bpm += 1; },
                        [&uiRef]() { uiRef.ctxt.bpm -= 1; },
                        []() {}
                    },
                    { //Toggle Metronome
                        [&uiRef]() { uiRef.ctxt.metronomeOn = !uiRef.ctxt.metronomeOn; },
                        [&uiRef]() { uiRef.ctxt.metronomeOn = !uiRef.ctxt.metronomeOn; },
                        []() {}
                    }
                },
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
