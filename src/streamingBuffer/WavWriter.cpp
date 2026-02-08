#include "../../include/streamingBuffer/WavWriter.h"
#include "../../include/general/ResourceManager.h"
#include <algorithm>
#include <limits>

int64_t MAX_FRAME_COUNT = std::numeric_limits<int64_t>::max();
//compiel

WavWriter::WavWriter(std::string filename, ResourceManager& resourceManager, sf_count_t totalFrames) {
    info.samplerate = resourceManager.audioFramesPerSecond;
    //TODO: stereo will fail among other reasons bc. the flush and stream buffers are made for mono, should then be interleaved.
    info.channels   = resourceManager.inMonoMode ? 1 : 2;
    info.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT; // 32‑bit float WAV
    frames = totalFrames;

    f = sf_open(filename.c_str(), SFM_WRITE, &info);
    if(!f) throw std::runtime_error(sf_strerror(nullptr));

    // Pre-size the file to known length
    /*
    if(sf_command(f, SFC_FILE_TRUNCATE, &frames, sizeof(frames)) != SF_TRUE) {
        // Fallback: write zeros
        //TODO: wouldnt it be better to directly write the correct values on the first go?
        std::vector<float> zeros(4096 * info.channels, 0.f);
        for(sf_count_t position = 0; position < frames; ) {
            sf_count_t batchLength = std::min<sf_count_t>(frames - position, zeros.size() / info.channels);
            sf_writef_float(f, zeros.data(), batchLength);//the write pointer automagically advances
            position += batchLength;
        }
    }
    */
    sf_seek(f, 0, SEEK_SET);
}

WavWriter::WavWriter(std::string filename, ResourceManager& resourceManager)
    : WavWriter(filename, resourceManager, MAX_FRAME_COUNT) {
    // Nothing to do here
}

void WavWriter::truncateFile(int finalFrames) {
    // Nothing to do here for now
    frames = finalFrames;
    auto result = sf_command(f, SFC_FILE_TRUNCATE, &frames, sizeof(frames));
    if(result != SF_TRUE) {
        rt_printf("WavWriter::truncateFile: truncated file to %lld frames\n", (long long)finalFrames);
    }
    else {
        std::string errmsg = "WavWriter::truncateFile errorcode: + " + std::to_string(result) + " with finalFrames=" + std::to_string(finalFrames);
        throw std::runtime_error(errmsg);
    }
}

WavWriter::WavWriter(WavWriter&& other) noexcept
    : f(other.f), info(other.info), frames(other.frames) {
    other.f = nullptr;
    other.info = SF_INFO{};
    other.frames = 0;
}

WavWriter& WavWriter::operator=(WavWriter&& other) noexcept {
    if(this != &other){
        if(f) sf_close(f);
        f = other.f;
        info = other.info;
        frames = other.frames;
        other.f = nullptr;
        other.info = SF_INFO{};
        other.frames = 0;
    }
    return *this;
}

// write interleaved chunk starting at frameOffset
void WavWriter::writeChunk(sf_count_t frameOffset, const std::vector<float>& interleaved, int framesToWrite) {
    if(framesToWrite < 0) {
        framesToWrite = interleaved.size() / info.channels;
    }
    if(frameOffset + framesToWrite > frames) throw std::out_of_range("chunk beyond file length");
    if(sf_seek(f, frameOffset, SEEK_SET) < 0) throw std::runtime_error("seek failed");
    sf_count_t written = sf_writef_float(f, interleaved.data(), framesToWrite);
    printf("WavWriter::writeChunk: wrote %lld where should print %lld frames at offset %lld\n", (long long)written, (long long)framesToWrite, (long long)frameOffset);
    if(written != framesToWrite) throw std::runtime_error("partial write");
}
