#include "../include/ADSR.h"
#include "../include/ResourceManager.h"
#include <cassert>

void ADSR::init() {
	assert(resourceManager != nullptr);
    //TODO: maybe put this into the constructor?
    attackIncrement = (attackTime > 0.0f) ? (1.0f / (attackTime * resourceManager->audioFramesPerSecond)) : 1.0f;
    decayDecrement  = (decayTime  > 0.0f) ? ((1.0f - sustainLevel) / (decayTime * resourceManager->audioFramesPerSecond)) : 1.0f;
    releaseDecrement= (releaseTime> 0.0f && sustainLevel > 0.0f) ? (sustainLevel / (releaseTime * resourceManager->audioFramesPerSecond)) : 1.0f;
}

float ADSR::process() {
    switch (state) {
        case adsrState::Idle:
            return 0.0f;

        case adsrState::Attack:
            value += attackIncrement;
            if (value >= 1.0f) {
                value = 1.0f;
                state = adsrState::Decay;
            }
            break;

        case adsrState::Decay:
            value -= decayDecrement;
            if (value <= sustainLevel) {
                value = sustainLevel;
                state = adsrState::Sustain;
            }
            break;

        case adsrState::Sustain:
            // hold steady
            break;

        case adsrState::Release: //TODO: better do this exponentially?
            value -= releaseDecrement;
            if (value <= 0.0f) {
                value = 0.0f;
                state = adsrState::Idle;
            }
            break;
    }
    return value;
}
