#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <unistd.h>

#include "../../include/general/DebugLog.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/streamingBuffer/StreamingBuffer.h"
#include "../../include/audio/SamplePack.h"
#include "../../include/audio/Voices.h"

namespace {
SamplePackVoiceSettings sanitizeVoiceSettings(const SamplePackVoiceSettings& in){
    SamplePackVoiceSettings out = in;
    if(out.attack < 0.0f) out.attack = 0.0f;
    if(out.decay < 0.0f) out.decay = 0.0f;
    if(out.sustain < 0.0f) out.sustain = 0.0f;
    if(out.sustain > 1.0f) out.sustain = 1.0f;
    if(out.release < 0.0f) out.release = 0.0f;
    return out;
}

std::string trimCopy(const std::string& in){
    size_t start = 0;
    while(start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))){
        ++start;
    }
    size_t end = in.size();
    while(end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))){
        --end;
    }
    return in.substr(start, end - start);
}

std::string toLowerCopy(const std::string& in){
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

int findColumnIndex(const std::vector<std::string>& header, const std::string& columnName){
    for(size_t i = 0; i < header.size(); ++i){
        if(header[i] == columnName){
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool fileExists(const std::string& path){
    return ::access(path.c_str(), F_OK) == 0;
}

bool startsWith(const std::string& value, const std::string& prefix){
    return value.size() >= prefix.size() &&
        value.compare(0, prefix.size(), prefix) == 0;
}

void appendUniquePath(std::vector<std::string>& paths, const std::string& candidate){
    if(candidate.empty()){
        return;
    }
    if(std::find(paths.begin(), paths.end(), candidate) != paths.end()){
        return;
    }
    paths.push_back(candidate);
}

void appendPathLayoutVariants(std::vector<std::string>& candidates, const std::string& path){
    appendUniquePath(candidates, path);

    // Relative entries usually start with `Samples/...`.
    if(startsWith(path, "Samples/")){
        const std::string rest = path.substr(std::string("Samples/").size());
        appendUniquePath(candidates, "/root/Bela/Samples/" + rest);
        appendUniquePath(candidates, "/root/Bela/projects/instrumentFromPC/Samples/" + rest);
    }

    // Some deployments place curated packs in `/root/Bela/Samples/<group>/...`
    // while set entries point to `Samples/instruments/<group>/...`.
    if(startsWith(path, "Samples/instruments/")){
        const std::string rest = path.substr(std::string("Samples/instruments/").size());
        appendUniquePath(candidates, "Samples/" + rest);
        appendUniquePath(candidates, "/root/Bela/Samples/" + rest);
        appendUniquePath(candidates, "/root/Bela/projects/instrumentFromPC/Samples/" + rest);
    }

    // Absolute Bela paths should also resolve when running locally.
    if(startsWith(path, "/root/Bela/Samples/")){
        const std::string rest = path.substr(std::string("/root/Bela/Samples/").size());
        appendUniquePath(candidates, "Samples/" + rest);
        appendUniquePath(candidates, "Samples/instruments/" + rest);
        appendUniquePath(candidates, "/root/Bela/projects/instrumentFromPC/Samples/" + rest);
        appendUniquePath(candidates, "/root/Bela/projects/instrumentFromPC/Samples/instruments/" + rest);
    }

    if(startsWith(path, "/root/Bela/Samples/instruments/")){
        const std::string rest = path.substr(std::string("/root/Bela/Samples/instruments/").size());
        appendUniquePath(candidates, "/root/Bela/Samples/" + rest);
        appendUniquePath(candidates, "Samples/" + rest);
        appendUniquePath(candidates, "Samples/instruments/" + rest);
        appendUniquePath(candidates, "/root/Bela/projects/instrumentFromPC/Samples/" + rest);
        appendUniquePath(candidates, "/root/Bela/projects/instrumentFromPC/Samples/instruments/" + rest);
    }

    if(startsWith(path, "/root/Bela/projects/instrumentFromPC/Samples/")){
        const std::string rest = path.substr(std::string("/root/Bela/projects/instrumentFromPC/Samples/").size());
        appendUniquePath(candidates, "Samples/" + rest);
        appendUniquePath(candidates, "/root/Bela/Samples/" + rest);
    }

    if(startsWith(path, "/root/Bela/projects/instrumentFromPC/Samples/instruments/")){
        const std::string rest = path.substr(std::string("/root/Bela/projects/instrumentFromPC/Samples/instruments/").size());
        appendUniquePath(candidates, "Samples/" + rest);
        appendUniquePath(candidates, "Samples/instruments/" + rest);
        appendUniquePath(candidates, "/root/Bela/Samples/" + rest);
        appendUniquePath(candidates, "/root/Bela/Samples/instruments/" + rest);
    }
}

std::string parentDirectory(const std::string& path){
    const size_t pos = path.find_last_of('/');
    if(pos == std::string::npos){
        return ".";
    }
    if(pos == 0){
        return "/";
    }
    return path.substr(0, pos);
}
}

SamplePack::SamplePack(ResourceManager& resourceManager,
            std::string samplePackName, std::string samplePackFolderPath, size_t bufferSizeInFrames, bool autoInitialize)
            : voices(resourceManager),
            samplePackName(std::move(samplePackName)),
            samplePackFolderPath(std::move(samplePackFolderPath)),
            vcslRootPath(resolveExistingPath("/root/Bela/Samples/VCSL-1.2.2-RC", "Samples/VCSL-1.2.2-RC")),
            tableRootPath(resolveExistingPath("/root/Bela/Samples/bela_tables/vcsl_full", "Samples/bela_tables/vcsl_full")),
            sampleFileMap(),
            availableSamples(),
            sampleIdentifierByRelPath(),
            attackZones(),
            releaseZones(),
            attackLookup(kLookupSize),
            releaseLookup(kLookupSize),
            attackRoundRobinCounter(kLookupSize, 0),
            releaseRoundRobinCounter(kLookupSize, 0),
            lastNoteVelocity(kMidiValueCount, 64),
            hasReleaseZones(false),
            streamingBuffer(resourceManager,
                bufferSizeInFrames,
                voices.maxVoices,
                this->samplePackName + "_buffer", this->vcslRootPath,
                availableSamples),
            initTaskName(this->samplePackName + "_ITask"),
            initTask(this, initTaskPrio, initTaskName)
{
    assert(!this->samplePackName.empty() && "SamplePack ctor samplePackName empty");
    assert(!this->samplePackFolderPath.empty() && "SamplePack ctor samplePackFolderPath empty");
    assert(bufferSizeInFrames > 0 && "SamplePack ctor bufferSizeInFrames must be > 0");
    assert(voices.maxVoices > 0 && "SamplePack ctor maxVoices must be positive");
    DEBUG_PRINTF("SamplePack ctor: name=%s instrument=%s tableRoot=%s sampleRoot=%s bufferFrames=%u\n",
        this->samplePackName.c_str(), this->samplePackFolderPath.c_str(),
        this->tableRootPath.c_str(), this->vcslRootPath.c_str(), (unsigned int)bufferSizeInFrames);

    streamingBuffer.setFolderPath(vcslRootPath);
    streamingBuffer.setSampleFileMap(&sampleFileMap);

    loading.store(true, std::memory_order_release);
    loadingIdleBlockCounter = 0;
    if(autoInitialize){
        initWork();
    } else {
        loading.store(false, std::memory_order_release);
    }
}

int SamplePack::clampMidiValue(int value){
    if(value < 0) return 0;
    if(value >= kMidiValueCount) return kMidiValueCount - 1;
    return value;
}

bool SamplePack::parseInt(const std::string& text, int& out){
    try {
        out = std::stoi(trimCopy(text));
        return true;
    } catch(...) {
        out = 0;
        return false;
    }
}

bool SamplePack::parseFloat(const std::string& text, float& out){
    try {
        out = std::stof(trimCopy(text));
        return true;
    } catch(...) {
        out = 0.0f;
        return false;
    }
}

std::vector<std::string> SamplePack::splitTabRow(const std::string& line){
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string cell;
    while(std::getline(ss, cell, '\t')){
        out.push_back(cell);
    }
    if(!line.empty() && line.back() == '\t'){
        out.push_back("");
    }
    return out;
}

size_t SamplePack::lookupIndex(int note, int velocity){
    return static_cast<size_t>(clampMidiValue(note)) * kMidiValueCount + static_cast<size_t>(clampMidiValue(velocity));
}

float SamplePack::dbToLinear(float db){
    if(db <= -90.0f){
        return 0.0f;
    }
    return std::pow(10.0f, db / 20.0f);
}

SampleIdentifier SamplePack::makeSampleIdentifier(size_t sampleSlot) const {
    const size_t maxSamples = static_cast<size_t>(kSampleIdentifierStride) * static_cast<size_t>(kSampleIdentifierStride);
    if(sampleSlot >= maxSamples){
        throw std::runtime_error("SamplePack: instrument has more than 16384 unique samples, unsupported");
    }
    const int first = static_cast<int>(sampleSlot / kSampleIdentifierStride);
    const int second = static_cast<int>(sampleSlot % kSampleIdentifierStride);
    return {first, second};
}

std::string SamplePack::resolveExistingPath(const std::string& preferred, const std::string& fallback) const {
    return (::access(preferred.c_str(), F_OK) == 0) ? preferred : fallback;
}

void SamplePack::taskWorkMessage(std::string& taskName, DefaultTaskMessage){
	if(taskName == samplePackName + "_ITask"){
		initWork();
		return;
	}
	throw std::runtime_error("SamplePack::taskWorkMessage invoked with task name " + taskName);
}

void SamplePack::initForFolder(std::string samplePackFolderPath){
    assert(!samplePackFolderPath.empty() && "SamplePack::initForFolder empty folder path");
    loading.store(true, std::memory_order_release);
    loadingIdleBlockCounter = 0;
    this->samplePackFolderPath = std::move(samplePackFolderPath);
    streamingBuffer.setFolderPath(vcslRootPath);
    streamingBuffer.setSampleFileMap(&sampleFileMap);
    DefaultTaskMessage msg;
    initTask.pushMessage(TaskMessageTarget::TaskThread, msg);
    initTask.taskCheckAndWorkMessages();
}

void SamplePack::clearZoneState(){
    availableSamples.clear();
    sampleFileMap.clear();
    sampleIdentifierByRelPath.clear();
    attackZones.clear();
    releaseZones.clear();
    hasReleaseZones = false;

    for(auto& cell : attackLookup){
        cell.clear();
    }
    for(auto& cell : releaseLookup){
        cell.clear();
    }
    std::fill(attackRoundRobinCounter.begin(), attackRoundRobinCounter.end(), 0u);
    std::fill(releaseRoundRobinCounter.begin(), releaseRoundRobinCounter.end(), 0u);
    std::fill(lastNoteVelocity.begin(), lastNoteVelocity.end(), 64);
}

std::string SamplePack::resolveZoneTablePath(const std::string& instrumentId) const{
    if(instrumentId.size() >= 10 && instrumentId.find(".zones.tsv") != std::string::npos){
        // Direct zones path provided by instrument set entry.
        // Resolve robustly across local/Bela cwd differences.
        std::vector<std::string> candidates;
        appendPathLayoutVariants(candidates, instrumentId);
        if(!instrumentId.empty() && instrumentId.front() != '/'){
            appendPathLayoutVariants(candidates, tableRootPath + "/" + instrumentId);
            appendPathLayoutVariants(candidates, "/root/Bela/" + instrumentId);
            appendPathLayoutVariants(candidates, "/root/Bela/projects/instrumentFromPC/" + instrumentId);
        }
        for(const auto& candidate : candidates){
            if(fileExists(candidate)){
                return candidate;
            }
        }
        return instrumentId;
    }

    const std::string indexPath = tableRootPath + "/instruments.tsv";
    std::ifstream indexFile(indexPath);
    if(!indexFile){
        throw std::runtime_error("SamplePack: cannot open instruments index at " + indexPath);
    }

    std::string line;
    if(!std::getline(indexFile, line)){
        throw std::runtime_error("SamplePack: instruments.tsv is empty: " + indexPath);
    }
    std::vector<std::string> header = splitTabRow(line);
    const int instrumentIdIndex = findColumnIndex(header, "instrument_id");
    const int zonesFileIndex = findColumnIndex(header, "zones_file");
    if(instrumentIdIndex < 0 || zonesFileIndex < 0){
        throw std::runtime_error("SamplePack: instruments.tsv missing required columns instrument_id/zones_file");
    }

    while(std::getline(indexFile, line)){
        if(trimCopy(line).empty()){
            continue;
        }
        const std::vector<std::string> row = splitTabRow(line);
        if(static_cast<int>(row.size()) <= std::max(instrumentIdIndex, zonesFileIndex)){
            continue;
        }
        if(row[static_cast<size_t>(instrumentIdIndex)] != instrumentId){
            continue;
        }
        const std::string zonesRelPath = trimCopy(row[static_cast<size_t>(zonesFileIndex)]);
        if(zonesRelPath.empty()){
            break;
        }
        return tableRootPath + "/" + zonesRelPath;
    }

    throw std::runtime_error("SamplePack: instrument_id not found in instruments.tsv: " + instrumentId);
}

void SamplePack::loadZonesFromFile(const std::string& zoneTablePath){
    std::ifstream zoneFile(zoneTablePath);
    if(!zoneFile){
        throw std::runtime_error("SamplePack: cannot open zones file " + zoneTablePath);
    }
    const std::string zoneTableDir = parentDirectory(zoneTablePath);
    const std::string zonePackRoot = parentDirectory(zoneTableDir);

    std::string line;
    if(!std::getline(zoneFile, line)){
        throw std::runtime_error("SamplePack: zones file is empty " + zoneTablePath);
    }
    const std::vector<std::string> header = splitTabRow(line);

    const int sampleIndex = findColumnIndex(header, "sample_relpath");
    const int lokeyIndex = findColumnIndex(header, "lokey");
    const int hikeyIndex = findColumnIndex(header, "hikey");
    const int lovelIndex = findColumnIndex(header, "lovel");
    const int hivelIndex = findColumnIndex(header, "hivel");
    const int keycenterIndex = findColumnIndex(header, "pitch_keycenter");
    const int tuneIndex = findColumnIndex(header, "tune_cents");
    const int volumeIndex = findColumnIndex(header, "volume_db");
    const int triggerIndex = findColumnIndex(header, "trigger");
    const int offsetIndex = findColumnIndex(header, "offset");
    const int ampVeltrackIndex = findColumnIndex(header, "amp_veltrack");

    if(sampleIndex < 0){
        throw std::runtime_error("SamplePack: zones file missing sample_relpath: " + zoneTablePath);
    }

    auto getCell = [](const std::vector<std::string>& row, int index) -> std::string {
        if(index < 0){
            return "";
        }
        const size_t safeIndex = static_cast<size_t>(index);
        if(safeIndex >= row.size()){
            return "";
        }
        return row[safeIndex];
    };

    while(std::getline(zoneFile, line)){
        if(trimCopy(line).empty()){
            continue;
        }
        const std::vector<std::string> row = splitTabRow(line);
        std::string sampleRelPath = trimCopy(getCell(row, sampleIndex));
        if(sampleRelPath.empty()){
            continue;
        }

        auto idIt = sampleIdentifierByRelPath.find(sampleRelPath);
        SampleIdentifier sampleIdentifier;
        if(idIt == sampleIdentifierByRelPath.end()){
            sampleIdentifier = makeSampleIdentifier(sampleIdentifierByRelPath.size());
            sampleIdentifierByRelPath[sampleRelPath] = sampleIdentifier;

            std::string absolutePath;
            if(!sampleRelPath.empty() && sampleRelPath.front() == '/'){
                absolutePath = sampleRelPath;
            } else {
                const std::string zoneRelativePath = zoneTableDir + "/" + sampleRelPath;
                const std::string packRelativePath = zonePackRoot + "/" + sampleRelPath;
                const std::string vcslRelativePath = vcslRootPath + "/" + sampleRelPath;
                if(fileExists(zoneRelativePath)){
                    absolutePath = zoneRelativePath;
                } else if(fileExists(packRelativePath)){
                    absolutePath = packRelativePath;
                } else {
                    absolutePath = vcslRelativePath;
                }
            }
            const int numFrames = AudioFileUtilities::getNumFrames(absolutePath);
            if(numFrames <= 0){
                rt_printf("SamplePack: unable to read frames for %s, skipping referenced zones\n", absolutePath.c_str());
                sampleIdentifierByRelPath.erase(sampleRelPath);
                continue;
            }
            sampleFileMap[sampleIdentifier] = absolutePath;
            availableSamples[sampleIdentifier] = static_cast<size_t>(numFrames);
        } else {
            sampleIdentifier = idIt->second;
            if(availableSamples.find(sampleIdentifier) == availableSamples.end()){
                continue;
            }
        }

        int lokey = 0;
        int hikey = 127;
        int lovel = 0;
        int hivel = 127;
        int keycenter = 60;
        int offset = 0;
        float tune = 0.0f;
        float volumeDb = 0.0f;
        float ampVeltrack = 100.0f;

        parseInt(getCell(row, lokeyIndex), lokey);
        parseInt(getCell(row, hikeyIndex), hikey);
        parseInt(getCell(row, lovelIndex), lovel);
        parseInt(getCell(row, hivelIndex), hivel);
        if(!parseInt(getCell(row, keycenterIndex), keycenter)){
            keycenter = lokey;
        }
        parseInt(getCell(row, offsetIndex), offset);
        parseFloat(getCell(row, tuneIndex), tune);
        parseFloat(getCell(row, volumeIndex), volumeDb);
        parseFloat(getCell(row, ampVeltrackIndex), ampVeltrack);

        lokey = clampMidiValue(lokey);
        hikey = clampMidiValue(hikey);
        if(hikey < lokey) hikey = lokey;
        lovel = clampMidiValue(lovel);
        hivel = clampMidiValue(hivel);
        if(hivel < lovel) hivel = lovel;
        keycenter = clampMidiValue(keycenter);
        if(offset < 0) offset = 0;

        ZoneEntry zone;
        zone.sampleIdentifier = sampleIdentifier;
        zone.lokey = lokey;
        zone.hikey = hikey;
        zone.lovel = lovel;
        zone.hivel = hivel;
        zone.pitchKeycenter = keycenter;
        zone.tuneCents = tune;
        zone.volumeDb = volumeDb;
        zone.ampVeltrack = std::max(0.0f, std::min(100.0f, ampVeltrack));
        zone.offset = offset;

        const std::string triggerValue = toLowerCopy(trimCopy(getCell(row, triggerIndex)));
        if(triggerValue.find(kReleaseTrigger) != std::string::npos){
            const size_t zoneIndex = releaseZones.size();
            if(zoneIndex > static_cast<size_t>(std::numeric_limits<uint16_t>::max())){
                throw std::runtime_error("SamplePack: too many release zones for uint16 lookup indices");
            }
            releaseZones.push_back(zone);
            for(int note = zone.lokey; note <= zone.hikey; ++note){
                for(int vel = zone.lovel; vel <= zone.hivel; ++vel){
                    releaseLookup[lookupIndex(note, vel)].push_back(static_cast<uint16_t>(zoneIndex));
                }
            }
        } else {
            const size_t zoneIndex = attackZones.size();
            if(zoneIndex > static_cast<size_t>(std::numeric_limits<uint16_t>::max())){
                throw std::runtime_error("SamplePack: too many attack zones for uint16 lookup indices");
            }
            attackZones.push_back(zone);
            for(int note = zone.lokey; note <= zone.hikey; ++note){
                for(int vel = zone.lovel; vel <= zone.hivel; ++vel){
                    attackLookup[lookupIndex(note, vel)].push_back(static_cast<uint16_t>(zoneIndex));
                }
            }
        }
    }

    hasReleaseZones = !releaseZones.empty();
}

bool SamplePack::parseLegacyFilename(const std::string& filename, int& key, int& velocity){
    key = 0;
    velocity = 0;
    if(filename.size() < 6){
        return false;
    }
    const std::string suffix = ".wav";
    if(filename.substr(filename.size() - suffix.size()) != suffix){
        return false;
    }

    const std::string stem = filename.substr(0, filename.size() - suffix.size());
    const size_t separator = stem.rfind('_');
    if(separator == std::string::npos){
        return false;
    }

    int parsedKey = 0;
    int parsedVelocity = 0;
    if(!parseInt(stem.substr(0, separator), parsedKey)){
        return false;
    }
    if(!parseInt(stem.substr(separator + 1), parsedVelocity)){
        return false;
    }
    if(parsedKey < 0 || parsedKey >= kMidiValueCount || parsedVelocity < 0 || parsedVelocity >= kMidiValueCount){
        return false;
    }

    key = parsedKey;
    velocity = parsedVelocity;
    return true;
}

bool SamplePack::loadLegacyNoteVelocityFolder(){
    if(samplePackFolderPath.empty()){
        return false;
    }
    if(::access(samplePackFolderPath.c_str(), F_OK) != 0){
        return false;
    }

    DIR* dir = opendir(samplePackFolderPath.c_str());
    if(!dir){
        return false;
    }

    bool loadedAny = false;
    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr){
        const std::string fileName = entry->d_name;
        int key = 0;
        int velocity = 0;
        if(!parseLegacyFilename(fileName, key, velocity)){
            continue;
        }

        const SampleIdentifier sampleIdentifier{key, velocity};
        const std::string fullPath = samplePackFolderPath + "/" + fileName;
        const int numFrames = AudioFileUtilities::getNumFrames(fullPath);
        if(numFrames <= 0){
            continue;
        }

        sampleFileMap[sampleIdentifier] = fullPath;
        availableSamples[sampleIdentifier] = static_cast<size_t>(numFrames);

        ZoneEntry zone;
        zone.sampleIdentifier = sampleIdentifier;
        zone.lokey = key;
        zone.hikey = key;
        zone.lovel = velocity;
        zone.hivel = velocity;
        zone.pitchKeycenter = key;
        zone.tuneCents = 0.0f;
        zone.volumeDb = 0.0f;
        zone.ampVeltrack = 100.0f;
        zone.offset = 0;
        const size_t zoneIndex = attackZones.size();
        attackZones.push_back(zone);
        attackLookup[lookupIndex(key, velocity)].push_back(static_cast<uint16_t>(zoneIndex));
        loadedAny = true;
    }

    closedir(dir);
    hasReleaseZones = false;
    return loadedAny;
}

void SamplePack::initWork(){
    streamingBuffer.clear();
    clearZoneState();
    getAvailableSamples();
    DEBUG_PRINTF("SamplePack::initWork instrument=%s usableSamples=%zu attackZones=%zu releaseZones=%zu\n",
        samplePackFolderPath.c_str(), availableSamples.size(), attackZones.size(), releaseZones.size());
    streamingBuffer.initializeForNewSamplePack(availableSamples);
}

std::unordered_map<SampleIdentifier, size_t>& SamplePack::getAvailableSamples(){
    if(!availableSamples.empty()){
        return availableSamples;
    }
    assert(!samplePackFolderPath.empty() && "SamplePack::getAvailableSamples empty instrument id");

    bool loaded = false;
    if(samplePackFolderPath.find('/') != std::string::npos){
        loaded = loadLegacyNoteVelocityFolder();
    }
    if(!loaded){
        const std::string zoneTablePath = resolveZoneTablePath(samplePackFolderPath);
        loadZonesFromFile(zoneTablePath);
        loaded = true;
    }

    if(availableSamples.empty()){
        throw std::runtime_error("SamplePack: no playable samples for instrument " + samplePackFolderPath);
    }
    if(attackZones.empty()){
        throw std::runtime_error("SamplePack: no attack zones for instrument " + samplePackFolderPath);
    }
    return availableSamples;
}

std::vector<std::string> SamplePack::listWavFiles() {
    std::vector<std::string> result;
    result.reserve(sampleFileMap.size());
    for(const auto& entry : sampleFileMap){
        result.push_back(entry.second);
    }
    std::sort(result.begin(), result.end());
    return result;
}

SampleIdentifier SamplePack::findClosestSample(SampleIdentifier sampleIdentifier){
    if(availableSamples.empty()){
        throw std::runtime_error("SamplePack::findClosestSample no available samples");
    }
    auto exactIt = availableSamples.find(sampleIdentifier);
    if(exactIt != availableSamples.end()){
        return sampleIdentifier;
    }
    SampleIdentifier best = availableSamples.begin()->first;
    int bestDistance = std::abs(sampleIdentifier.first - best.first) + std::abs(sampleIdentifier.second - best.second);
    for(const auto& sample : availableSamples){
        const int distance =
            std::abs(sampleIdentifier.first - sample.first.first) +
            std::abs(sampleIdentifier.second - sample.first.second);
        if(distance < bestDistance){
            bestDistance = distance;
            best = sample.first;
        }
    }
    return best;
}

const SamplePack::ZoneIndexList& SamplePack::resolveZoneIndices(int note, int velocity, bool release) const {
    static ZoneIndexList empty;
    const std::vector<ZoneIndexList>& lookup = release ? releaseLookup : attackLookup;
    note = clampMidiValue(note);
    velocity = clampMidiValue(velocity);

    const ZoneIndexList& direct = lookup[lookupIndex(note, velocity)];
    if(!direct.empty()){
        return direct;
    }

    for(int radius = 1; radius < kMidiValueCount; ++radius){
        const int noteLow = std::max(0, note - radius);
        const int noteHigh = std::min(kMidiValueCount - 1, note + radius);
        const int velLow = std::max(0, velocity - radius);
        const int velHigh = std::min(kMidiValueCount - 1, velocity + radius);
        for(int n = noteLow; n <= noteHigh; ++n){
            for(int v = velLow; v <= velHigh; ++v){
                const ZoneIndexList& candidate = lookup[lookupIndex(n, v)];
                if(!candidate.empty()){
                    return candidate;
                }
            }
        }
    }

    return empty;
}

void SamplePack::triggerZone(const ZoneEntry& zone, int note, int midiVelocity, bool gainFromVelocity, float explicitGain){
    const float semitoneDelta =
        static_cast<float>(note - zone.pitchKeycenter) +
        (zone.tuneCents / 100.0f);
    const float playbackRate = std::pow(2.0f, semitoneDelta / 12.0f);

    voices.prepareForNewVoice();
    StreamingBufferIterator& iterator = streamingBuffer.begin(zone.sampleIdentifier, SBIType::Read, playbackRate);
    if(zone.offset > 0){
        iterator.seek(static_cast<size_t>(zone.offset));
    }

    float gain = explicitGain;
    if(gainFromVelocity){
        const float velocityNorm = static_cast<float>(clampMidiValue(midiVelocity)) / 127.0f;
        const float velTrack = std::max(0.0f, std::min(zone.ampVeltrack / 100.0f, 1.0f));
        gain = (1.0f - velTrack) + (velTrack * velocityNorm);
    }
    gain *= dbToLinear(zone.volumeDb);

    voices.triggerVoice(iterator, note, playbackRate, gain,
        voiceSettings.repeat,
        voiceSettings.attack,
        voiceSettings.decay,
        voiceSettings.sustain,
        voiceSettings.release);
}

void SamplePack::triggerVoice(int note, int midiVelocity, bool gainFromVelocity, float explicitGain){
    if(availableSamples.empty()){
        return;
    }
    note = clampMidiValue(note);
    midiVelocity = clampMidiValue(midiVelocity);
    if(midiVelocity <= 0){
        return;
    }
    if(!gainFromVelocity && explicitGain < 0.0f){
        return;
    }

    lastNoteVelocity[static_cast<size_t>(note)] = midiVelocity;
    const ZoneIndexList& zoneIndices = resolveZoneIndices(note, midiVelocity, false);
    if(zoneIndices.empty()){
        return;
    }

    const size_t rrCell = lookupIndex(note, midiVelocity);
    uint32_t& rr = attackRoundRobinCounter[rrCell];
    const uint16_t zoneIndex = zoneIndices[static_cast<size_t>(rr % zoneIndices.size())];
    ++rr;
    if(zoneIndex >= attackZones.size()){
        return;
    }
    triggerZone(attackZones[zoneIndex], note, midiVelocity, gainFromVelocity, explicitGain);
}

void SamplePack::triggerOff(int note){
    note = clampMidiValue(note);

    if(hasReleaseZones){
        const int releaseVelocity = lastNoteVelocity[static_cast<size_t>(note)];
        const ZoneIndexList& releaseZoneIndices = resolveZoneIndices(note, releaseVelocity, true);
        if(!releaseZoneIndices.empty()){
            const size_t rrCell = lookupIndex(note, releaseVelocity);
            uint32_t& rr = releaseRoundRobinCounter[rrCell];
            const uint16_t zoneIndex = releaseZoneIndices[static_cast<size_t>(rr % releaseZoneIndices.size())];
            ++rr;
            if(zoneIndex < releaseZones.size()){
                triggerZone(releaseZones[zoneIndex], note, releaseVelocity, true, 1.0f);
            }
        }
    }

    if(voiceSettings.repeat || voiceSettings.release > 0.0f){
        voices.triggerOff(note);
    }
}

int SamplePack::midiToSampleVelocity(int midiVelocity){
    return clampMidiValue(midiVelocity);
}

void SamplePack::setVoiceSettings(const SamplePackVoiceSettings& settings){
    voiceSettings = sanitizeVoiceSettings(settings);
}

void SamplePack::processBlockwise(){
    streamingBuffer.processBlockwise();
    if(loading.load(std::memory_order_acquire)){
        const bool initBusy = initTask.isInFlight();
        const bool streamBusy = streamingBuffer.streamerIsInFlight();
        if(!initBusy && !streamBusy){
            loadingIdleBlockCounter++;
            if(loadingIdleBlockCounter >= 2){
                loading.store(false, std::memory_order_release);
                loadingIdleBlockCounter = 0;
            }
        } else {
            loadingIdleBlockCounter = 0;
        }
    }
}

float SamplePack::process(){
    return voices.process();
}
