#include "../../include/audio/ADSR.h"
#include "../../include/general/ResourceManager.h"
#include <cassert>
//compiel
ADSR::ADSR(ADSR&& other) noexcept
	: attackTime(other.attackTime),
	  decayTime(other.decayTime),
	  sustainLevel(other.sustainLevel),
	  releaseTime(other.releaseTime),
	  resourceManager(other.resourceManager),
	  value(other.value),
	  attackIncrement(other.attackIncrement),
	  decayDecrement(other.decayDecrement),
	  releaseDecrement(other.releaseDecrement),
	  state(other.state) {}

ADSR& ADSR::operator=(ADSR&& other) noexcept {
	if(this != &other) {
		assert(&resourceManager == &other.resourceManager);
		attackTime = other.attackTime;
		decayTime = other.decayTime;
		sustainLevel = other.sustainLevel;
		releaseTime = other.releaseTime;
		value = other.value;
		attackIncrement = other.attackIncrement;
		decayDecrement = other.decayDecrement;
		releaseDecrement = other.releaseDecrement;
		state = other.state;
	}
	return *this;
}

void ADSR::init() {
    //TODO: maybe put this into the constructor?
    attackIncrement = (attackTime > 0.0f) ? (1.0f / (attackTime * resourceManager.audioFramesPerSecond)) : 1.0f;
    decayDecrement  = (decayTime  > 0.0f) ? ((1.0f - sustainLevel) / (decayTime * resourceManager.audioFramesPerSecond)) : 1.0f;
    releaseDecrement= (releaseTime> 0.0f && sustainLevel > 0.0f) ? (sustainLevel / (releaseTime * resourceManager.audioFramesPerSecond)) : 1.0f;
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
