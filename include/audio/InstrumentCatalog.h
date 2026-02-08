// include/audio/InstrumentCatalog.h
#pragma once
#include <string>
#include <vector>
//compile

struct InstrumentInfo {
  std::string displayName; // UI label
  std::string folderName;  // SD card folder
};

class InstrumentCatalog {
public:
    std::string& getDisplayName(size_t index);

    std::string& getFolderName(size_t index);

    int size(){return catalog.size();}

private:
    std::vector<InstrumentInfo> catalog =
    {
        {"Piano", "/root/Bela/Samples/Piano"},
        {"Std_Bass", "/root/Bela/Samples/Standard_Bass"},
        {"Sine", "/root/Bela/Samples/SineOscillator"},
        {"Saw", "/root/Bela/Samples/SawOscillator"},
        {"Square", "/root/Bela/Samples/SquareOscillator"}
    };
};

class DrumCatalog {
public:
    std::string& getDisplayName(size_t index);

    std::string& getFolderName(size_t index);

    int size(){return catalog.size();}
private:
    std::vector<InstrumentInfo> catalog =
    {
        {"808", "/root/Bela/Samples/drumkit_808"}
    };
};
