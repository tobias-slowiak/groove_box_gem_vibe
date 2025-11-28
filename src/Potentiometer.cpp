#include <vector>
#include <algorithm>
#include <cmath>
#include <cassert>
//test
#include "../include/BasicUtilities.h"
#include "../include/Potentiometer.h"
#include "../include/DebugLog.h"

namespace {
constexpr float kPotStep = 1.0f / 127.0f;
constexpr float kAnalogMax = 3.3f / 4.096f;
constexpr float kWarmupSeconds = 1.0f;
}

Potentiometer::Potentiometer(BelaContext* context, bool reverse, int pinNumber, float tau_ms)
	: context(context), reverse(reverse), pinNumber(pinNumber) {
	float tau = tau_ms * 1e-3f;

	if(context) {
		float blocksPerSecond = context->audioSampleRate / context->audioFrames;
		float Tc = 1.0f / blocksPerSecond;
		a = 1.0f - std::exp(-Tc / std::max(tau, 1e-6f));

		warmupBlocks = std::max(0, static_cast<int>(kWarmupSeconds * blocksPerSecond));
		warmupActive = warmupBlocks > 0;
		DEBUG_RT_PRINTF("Potentiometer %d init: reverse=%d tau_ms=%.2f warmupBlocks=%d\n",
		                pinNumber,
		                reverse ? 1 : 0,
		                tau_ms,
		                warmupBlocks);
	} else {
		a = 0.0f;
		warmupBlocks = 0;
		warmupActive = false;
	}
}

void Potentiometer::processBlockwise() {
	if(!context) return;

	float inValue = analogRead(context, 0, pinNumber);
	float mappedValue = map(inValue, 0.0f, kAnalogMax, 0.0f, 1.0f);
	if(reverse) mappedValue = 1.0f - mappedValue;
	mappedValue = clamp(mappedValue, 0.0f, 1.0f);

	assert(lastRawValues.size() > 1);
	float previousRawValueOne = lastRawValues.at(1);
	assert(lastRawValues.size() > 2);
	lastRawValues.at(2) = previousRawValueOne;
	assert(lastRawValues.size() > 0);
	float previousRawValueZero = lastRawValues.at(0);
	assert(lastRawValues.size() > 1);
	lastRawValues.at(1) = previousRawValueZero;
	assert(lastRawValues.size() > 0);
	lastRawValues.at(0) = mappedValue;

	assert(lastRawValues.size() > 0);
	float medianCandidateZero = lastRawValues.at(0);
	assert(lastRawValues.size() > 1);
	float medianCandidateOne = lastRawValues.at(1);
	assert(lastRawValues.size() > 2);
	float medianCandidateTwo = lastRawValues.at(2);
    float medianRaw = median3(medianCandidateZero, medianCandidateOne, medianCandidateTwo);
	filteredValue += a * (medianRaw - filteredValue);
	filteredValue = clamp(filteredValue, 0.0f, 1.0f);

	float normalized = filteredValue;
	int newStep = normalized >= 1.0f ? 127 : static_cast<int>(normalized * 127.0f);
	float quantizedValue = static_cast<float>(newStep) * kPotStep;

	if(warmupActive){
		if(warmupBlocks > 0){
			--warmupBlocks;
			if(warmupBlocks == 0){
				DEBUG_RT_PRINTF("Potentiometer %d warmup finished\n", pinNumber);
			}
			return;
		}
		warmupActive = false;
		lastStep = newStep;
		reportedValue = quantizedValue;
		pendingChange = true;
		DEBUG_RT_PRINTF("Potentiometer %d primed at step %d (%.3f)\n",
		                pinNumber,
		                newStep,
		                reportedValue);
		return;
	}

	if(newStep != lastStep) {
		lastStep = newStep;
		reportedValue = quantizedValue;
		pendingChange = true;
		/*
		DEBUG_RT_PRINTF("Potentiometer %d change: step %d value %.3f (filtered %.3f raw %.3f)\n",
		                pinNumber,
		                newStep,
		                reportedValue,
		                filteredValue,
		                mappedValue);
						*/
	}
}

bool Potentiometer::hasChanged(){
	if(pendingChange){
		pendingChange = false;
		return true;
	}
	return false;
}
