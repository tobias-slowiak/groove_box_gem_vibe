#include "SampledInstrument.h"

SampledInstrument::SampledInstrument() {
    emptySample.assign(1, 0.0f);
}

const std::vector<float>& SampledInstrument::getSample(int midiNote) const {
    auto it = samples.find(midiNote);
    if (it != samples.end()) {
        return it->second;
    }
    return emptySample;
}

void SampledInstrument::requestSamplePackLoad(const std::string& packPath) {
    std::lock_guard<std::mutex> guard(pendingLoadMutex);
    pendingPackPath = packPath;
}

void SampledInstrument::loadSamplesAuxTask(void* context) {
    if (auto* instrument = static_cast<SampledInstrument*>(context)) {
        instrument->performPendingLoad();
    }
}

void SampledInstrument::performPendingLoad() {
    std::string packPathCopy;
    {
        std::lock_guard<std::mutex> guard(pendingLoadMutex);
        if (pendingPackPath.empty()) {
            return;
        }
        packPathCopy.swap(pendingPackPath);
    }

    // TODO: Load the samples for the requested pack and populate samples.
    (void)packPathCopy;
}
