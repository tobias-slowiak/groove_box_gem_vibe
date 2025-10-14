#pragma once

#include <Bela.h>
#include <vector>

class Potentiometer {
public:
	Potentiometer() {}
    Potentiometer(BelaContext* context, bool reverse, int pinNumber, float tau_ms = 20.0f);

    void processBlockwise();
    
    int getPin(){ return pinNumber;}
    
    float getValue(){ return reportedValue;}
    
    bool hasChanged();
    
private:
	BelaContext* context = nullptr;
	bool reverse = false;
	int pinNumber = 0;
	float filteredValue = 0.0f;
	std::vector<float> lastRawValues = std::vector<float>(3, 0.0f);
    float a = 0.0f;           // smoothing coeff at control-rate
    bool pendingChange = false;
    float reportedValue = 0.0f;
    int lastStep = -1;
    int warmupBlocks = 0;
    bool warmupActive = false;
};
