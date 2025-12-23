#pragma once

#include "../ui/UIState.h"


class Mixer{
public:
    float process(){
        return 1.0f;
    }

    float inputGain;
    
    float instrumentGain;
    float looperGain;
    float samplerGain;

    float masterGain;
};