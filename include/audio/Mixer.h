#pragma once

#include "../general/BasicUtilities.h"
//compile

enum class GainId{
    Master,
    Instrument,
    Metronome,
    Looper,
    Sampler,
    InputL,
    InputR,
    COUNT
};

class Mixer{
public:
    Mixer(){
        for(int i = 0; i < (int)GainId::COUNT; i++){
            gainz.push_back(1.0f);
        }
    }

    float mix(std::vector<float>& frames){
        float frame = 0.0f;
        for(int i = 0; i < (int)GainId::COUNT; i++){
            frame += VEC_AT(gainz, i) * VEC_AT(frames, i);
        }
        return frame;
    }

    void setGain(GainId gainId, float value){
        VEC_AT(gainz, (int)gainId);
    }

    float getGain(GainId gainId){
        return VEC_AT(gainz, (int)gainId);
    }

private:
    std::vector<float> gainz;
};