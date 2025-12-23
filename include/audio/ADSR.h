#pragma once
#include<Bela.h>

class ResourceManager;

enum class adsrState {
	Idle,
	Attack,
	Decay,
	Sustain,
	Release
};

class ADSR {
public:
	ADSR() = delete;
    ADSR(float a, float d, float sLevel, float r, ResourceManager& resourceManager)
    	: attackTime(a), decayTime(d), sustainLevel(sLevel), releaseTime(r), resourceManager(resourceManager), state(adsrState::Attack) {}

	ADSR(ADSR&& other) noexcept;
	ADSR& operator=(ADSR&& other) noexcept;

	ADSR(const ADSR& other) = delete;
	ADSR& operator=(const ADSR& other) = delete;
    
    void init();

    void noteOff() { state = adsrState::Release; }
    
    bool isOn(){ return state != adsrState::Idle; }
    
    void instantOff(){ state = adsrState::Idle; }

    float process();

private:
    float attackTime = 0.01f;  // seconds
    float decayTime = 0.1f;
    float sustainLevel = 0.7f;
    float releaseTime = 0.2f;
    
    ResourceManager& resourceManager;

    float value = 0.0f;  // current envelope value

    float attackIncrement = 0.0f;
    float decayDecrement = 0.0f;
    float releaseDecrement = 0.0f;

    adsrState state = adsrState::Attack;
};
