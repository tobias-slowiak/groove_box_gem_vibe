#pragma once

#include <libraries/sndfile/sndfile.h>
#include <string>
#include <vector>


class ResourceManager;

class WavWriter {
public:

    WavWriter(std::string path, ResourceManager& resourceManager, sf_count_t totalFrames);

    void writeChunk(sf_count_t frameOffset, const std::vector<float>& interleaved);

    ~WavWriter() { if(f) sf_close(f); }

private:
    SNDFILE* f = nullptr;
    SF_INFO info{};
    sf_count_t frames = 0;
};
