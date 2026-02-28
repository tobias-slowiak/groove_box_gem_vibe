#include "../../include/audio/InstrumentSetLibrary.h"
#include "../../include/general/BasicUtilities.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
std::vector<std::string> splitTabRow(const std::string& line){
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

bool ensureDir(const char* path){
    if(::access(path, F_OK) == 0){
        return true;
    }
    if(::mkdir(path, 0755) == 0){
        return true;
    }
    return errno == EEXIST;
}

std::string chooseStoragePath(){
    if(::access("/root/Bela/Samples", F_OK) == 0){
        ensureDir("/root/Bela/Samples/data");
        return "/root/Bela/Samples/data/instrument_sets.txt";
    }
    ensureDir("Samples");
    ensureDir("Samples/data");
    return "Samples/data/instrument_sets.txt";
}

std::string chooseLegacyStoragePath(){
    if(::access("/root/Bela/Samples", F_OK) == 0){
        return "/root/Bela/Samples/instrument_sets.txt";
    }
    return "Samples/instrument_sets.txt";
}
}

InstrumentSetLibrary::InstrumentSetLibrary()
: storagePath(chooseStoragePath()){
    loadFromFile();
    if(sets.empty()){
        const std::string primaryStoragePath = storagePath;
        const std::string legacyStoragePath = chooseLegacyStoragePath();
        if(legacyStoragePath != primaryStoragePath){
            storagePath = legacyStoragePath;
            loadFromFile();
            storagePath = primaryStoragePath;
            if(!sets.empty()){
                saveToFile();
            }
        }
    }
    ensureAtLeastOneSet();
    activeSetIndex = clampSetIndex(activeSetIndex);
    saveToFile();
}

int InstrumentSetLibrary::getSetCount() const{
    return static_cast<int>(sets.size());
}

int InstrumentSetLibrary::clampSetIndex(int index) const{
    if(sets.empty()){
        return 0;
    }
    int count = static_cast<int>(sets.size());
    int wrapped = index % count;
    if(wrapped < 0){
        wrapped += count;
    }
    return wrapped;
}

int InstrumentSetLibrary::clampEntryIndex(int setIndex, int entryIndex) const{
    if(sets.empty()){
        return 0;
    }
    setIndex = clampSetIndex(setIndex);
    const int count = static_cast<int>(sets.at(static_cast<size_t>(setIndex)).entries.size());
    if(count <= 0){
        return 0;
    }
    int wrapped = entryIndex % count;
    if(wrapped < 0){
        wrapped += count;
    }
    return wrapped;
}

int InstrumentSetLibrary::getActiveSetIndex() const{
    return activeSetIndex;
}

void InstrumentSetLibrary::setActiveSetIndex(int index){
    int clamped = clampSetIndex(index);
    if(clamped == activeSetIndex){
        return;
    }
    activeSetIndex = clamped;
    saveToFile();
}

const std::string& InstrumentSetLibrary::getSetName(int setIndex) const{
    return sets.at(static_cast<size_t>(clampSetIndex(setIndex))).name;
}

void InstrumentSetLibrary::renameSet(int setIndex, const std::string& newName){
    setIndex = clampSetIndex(setIndex);
    std::string sanitized = sanitizeToken(trimCopy(newName));
    if(sanitized.empty()){
        sanitized = "Set";
    }
    sets.at(static_cast<size_t>(setIndex)).name = sanitized;
    saveToFile();
}

int InstrumentSetLibrary::getEntryCount(int setIndex) const{
    if(sets.empty()){
        return 0;
    }
    setIndex = clampSetIndex(setIndex);
    return static_cast<int>(sets.at(static_cast<size_t>(setIndex)).entries.size());
}

const InstrumentSetEntry& InstrumentSetLibrary::getEntry(int setIndex, int entryIndex) const{
    setIndex = clampSetIndex(setIndex);
    entryIndex = clampEntryIndex(setIndex, entryIndex);
    return sets.at(static_cast<size_t>(setIndex)).entries.at(static_cast<size_t>(entryIndex));
}

bool InstrumentSetLibrary::addOrUpdateEntry(int setIndex, const std::string& instrumentId, const std::string& displayName){
    if(instrumentId.empty()){
        return false;
    }
    setIndex = clampSetIndex(setIndex);
    std::vector<InstrumentSetEntry>& entries = sets.at(static_cast<size_t>(setIndex)).entries;
    const std::string safeInstrumentId = sanitizeToken(trimCopy(instrumentId));
    std::string safeDisplayName = sanitizeToken(trimCopy(displayName));
    if(safeDisplayName.empty()){
        safeDisplayName = safeInstrumentId;
    }

    for(auto& entry : entries){
        if(entry.instrumentId == safeInstrumentId){
            entry.displayName = safeDisplayName;
            saveToFile();
            return true;
        }
    }

    entries.push_back({safeInstrumentId, safeDisplayName});
    saveToFile();
    return true;
}

bool InstrumentSetLibrary::removeEntryByInstrumentId(int setIndex, const std::string& instrumentId){
    if(sets.empty() || instrumentId.empty()){
        return false;
    }
    setIndex = clampSetIndex(setIndex);
    std::vector<InstrumentSetEntry>& entries = sets.at(static_cast<size_t>(setIndex)).entries;
    const size_t oldSize = entries.size();
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const InstrumentSetEntry& entry){
        return entry.instrumentId == instrumentId;
    }), entries.end());
    if(entries.size() != oldSize){
        saveToFile();
        return true;
    }
    return false;
}

void InstrumentSetLibrary::addSet(const std::string& name){
    std::string sanitized = sanitizeToken(trimCopy(name));
    if(sanitized.empty()){
        sanitized = "Set " + std::to_string(static_cast<int>(sets.size()) + 1);
    }
    sets.push_back({sanitized, {}});
    activeSetIndex = clampSetIndex(static_cast<int>(sets.size()) - 1);
    saveToFile();
}

void InstrumentSetLibrary::removeSet(int setIndex){
    if(sets.size() <= 1){
        return;
    }
    setIndex = clampSetIndex(setIndex);
    sets.erase(sets.begin() + setIndex);
    if(activeSetIndex >= static_cast<int>(sets.size())){
        activeSetIndex = static_cast<int>(sets.size()) - 1;
    }
    activeSetIndex = clampSetIndex(activeSetIndex);
    ensureAtLeastOneSet();
    saveToFile();
}

void InstrumentSetLibrary::loadFromFile(){
    sets.clear();
    activeSetIndex = 0;

    std::ifstream file(storagePath);
    if(!file){
        return;
    }

    std::string line;
    InstrumentSet* currentSet = nullptr;
    while(std::getline(file, line)){
        const std::string trimmed = trimCopy(line);
        if(trimmed.empty() || (!trimmed.empty() && trimmed[0] == '#')){
            continue;
        }

        const std::vector<std::string> tokens = splitTabRow(trimmed);
        if(tokens.empty()){
            continue;
        }
        const std::string tag = trimCopy(tokens[0]);
        if(tag == "active_set"){
            if(tokens.size() >= 2){
                try {
                    activeSetIndex = std::stoi(trimCopy(tokens[1]));
                } catch(...) {
                    activeSetIndex = 0;
                }
            }
            continue;
        }
        if(tag == "set"){
            std::string setName = (tokens.size() >= 2) ? sanitizeToken(trimCopy(tokens[1])) : "";
            if(setName.empty()){
                setName = "Set";
            }
            sets.push_back({setName, {}});
            currentSet = &sets.back();
            continue;
        }
        if(tag == "entry" && currentSet != nullptr && tokens.size() >= 3){
            const std::string instrumentId = sanitizeToken(trimCopy(tokens[1]));
            std::string displayName = sanitizeToken(trimCopy(tokens[2]));
            if(instrumentId.empty()){
                continue;
            }
            if(displayName.empty()){
                displayName = instrumentId;
            }
            currentSet->entries.push_back({instrumentId, displayName});
        }
    }
}

void InstrumentSetLibrary::saveToFile() const{
    ensureDir("Samples");
    ensureDir("Samples/data");
    if(storagePath == "/root/Bela/Samples/data/instrument_sets.txt"){
        ensureDir("/root/Bela/Samples/data");
    }

    std::ofstream file(storagePath, std::ios::trunc);
    if(!file){
        if(storagePath != "Samples/data/instrument_sets.txt"){
            std::ofstream fallback("Samples/data/instrument_sets.txt", std::ios::trunc);
            if(!fallback){
                return;
            }
            fallback << "active_set\t" << clampSetIndex(activeSetIndex) << "\n";
            for(const auto& set : sets){
                fallback << "set\t" << sanitizeToken(set.name) << "\n";
                for(const auto& entry : set.entries){
                    fallback << "entry\t" << sanitizeToken(entry.instrumentId) << "\t" << sanitizeToken(entry.displayName) << "\n";
                }
            }
            return;
        }
        return;
    }

    file << "active_set\t" << clampSetIndex(activeSetIndex) << "\n";
    for(const auto& set : sets){
        file << "set\t" << sanitizeToken(set.name) << "\n";
        for(const auto& entry : set.entries){
            file << "entry\t" << sanitizeToken(entry.instrumentId) << "\t" << sanitizeToken(entry.displayName) << "\n";
        }
    }
}

void InstrumentSetLibrary::ensureAtLeastOneSet(){
    if(!sets.empty()){
        return;
    }
    sets.push_back({"Default", {}});
}

std::string InstrumentSetLibrary::sanitizeToken(const std::string& in) const{
    std::string out;
    out.reserve(in.size());
    for(char c : in){
        if(c == '\n' || c == '\r' || c == '\t'){
            out.push_back(' ');
        } else {
            out.push_back(c);
        }
    }
    return trimCopy(out);
}

std::string InstrumentSetLibrary::trimCopy(const std::string& in) const{
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
