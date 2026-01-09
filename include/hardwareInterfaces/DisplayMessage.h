#pragma once
//compiel
#include "IDisplayContext.h"

struct DisplayMessage{
    std::vector<std::vector<std::string>> lines;
    int progressDisplay;
    float progress;
    std::vector<std::vector<TextFrame>> textFrames;
};
