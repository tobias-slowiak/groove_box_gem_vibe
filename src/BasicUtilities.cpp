#include <math.h>
#include <algorithm>

#include "../include/BasicUtilities.h"

bool floatIsEqual(float a, float b){
	float floatEpsilon = 0.001;
	return fabs(a - b) < floatEpsilon;
}

float median3(float a, float b, float c) {
    return std::max(std::min(a,b), std::min(std::max(a,b),c));
}

float clamp(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}