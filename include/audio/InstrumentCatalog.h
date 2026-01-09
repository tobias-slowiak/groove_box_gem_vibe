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
        {"Piano", "/mnt/sdcard/Samples/Piano"},
        {"Std_Bass", "/mnt/sdcard/Samples/Standard_Bass"}
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
        {"808", "/mnt/sdcard/Samples/drumkit_808"}
    };
};
