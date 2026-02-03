 #include "../../include/ui/UI.h"
 #include "../../include/general/ResourceManager.h"
 #include "../../include/audio/Loopers.h"
 #include <string>
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
                        {"AnalogOut",
                        [&uiRef]() { return uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Analog1) ? "On" : "Off"; },
                            {
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::Metronome, Output::Analog1, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Analog1)); },
                                [&uiRef]() { uiRef.ctxt.rm.getSignalRouter().setOutput(Signal::Metronome, Output::Analog1, !uiRef.ctxt.rm.getSignalRouter().getOutput(Signal::Metronome, Output::Analog1)); },
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