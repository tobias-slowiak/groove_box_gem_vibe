// include/audio/InstrumentCatalog.h
#pragma once
#include <string>
#include <vector>

struct InstrumentInfo {
  std::string displayName; // UI label
  std::string folderName;  // SD card folder
};

class InstrumentCatalog {
public:
    std::string& getDisplayName(size_t index);

    std::string& getFolderName(size_t index);

private:
    std::vector<InstrumentInfo> catalog =
    {
        {"Piano", "/mnt/sdcard/Samples/Piano"},
        {"Std_Bass", "/mnt/sdcard/Samples/Standard_Bass"}
    };
};
