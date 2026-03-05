#pragma once
//compiel
#include "IDisplayContext.h"

struct DisplayMessage{
    std::vector<std::vector<std::string>> lines;
    int progressDisplay;
    float progress;
    std::vector<std::vector<TextFrame>> textFrames;
    std::vector<ScrollBar> scrollBars;
    bool tickOnly = false;
    bool recoverDisplay = false;
    bool waveformOverlayEnabled = false;
    std::vector<float> waveform;
    float waveformSliceStartNormalized = 0.0f;
    float waveformSliceEndNormalized = 1.0f;
    bool waveformEditStartBoundary = true;
};
