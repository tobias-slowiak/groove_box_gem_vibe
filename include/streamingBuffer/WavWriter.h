#pragma once
//compiel
#include <libraries/sndfile/sndfile.h>
#include <string>
#include <vector>


class ResourceManager;

class WavWriter {
public:

    WavWriter(std::string path, ResourceManager& resourceManager, sf_count_t totalFrames);
    WavWriter(std::string path, ResourceManager& resourceManager); //total frames unknown during construction
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;
    WavWriter(WavWriter&& other) noexcept;
    WavWriter& operator=(WavWriter&& other) noexcept;

    void writeChunk(sf_count_t frameOffset, const std::vector<float>& interleaved, int framesToWrite = -1);

    void truncateFile(int finalFrames);

    ~WavWriter() { if(f) sf_close(f); }

private:
    SNDFILE* f = nullptr;
    SF_INFO info{};
    sf_count_t frames = 0;
};
