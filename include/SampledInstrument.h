#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class SampledInstrument {
public:
    SampledInstrument();

    const std::vector<float>& getSample(int midiNote) const;

    void requestSamplePackLoad(const std::string& packPath);

    static void loadSamplesAuxTask(void* context);

private:
    void performPendingLoad();

    std::unordered_map<int, std::vector<float>> samples;
    std::vector<float> emptySample;
    std::string pendingPackPath;
    mutable std::mutex pendingLoadMutex;
};
