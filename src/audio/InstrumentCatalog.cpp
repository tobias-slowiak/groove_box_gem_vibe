#include "../../include/audio/InstrumentCatalog.h"
#include "../../include/general/BasicUtilities.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unordered_map>
#include <unistd.h>

namespace {
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

int findColumnIndex(const std::vector<std::string>& header, const std::string& columnName){
    for(size_t i = 0; i < header.size(); ++i){
        if(header[i] == columnName){
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool parseInt(const std::string& text, int& out){
    try {
        out = std::stoi(trimCopy(text));
        return true;
    } catch(...) {
        out = 0;
        return false;
    }
}

std::string resolveExistingPath(const std::string& preferred, const std::string& fallback){
    return (::access(preferred.c_str(), F_OK) == 0) ? preferred : fallback;
}

bool ensureDir(const std::string& path){
    if(path.empty()){
        return false;
    }
    if(::access(path.c_str(), F_OK) == 0){
        return true;
    }
    if(::mkdir(path.c_str(), 0755) == 0){
        return true;
    }
    return errno == EEXIST;
}

bool ensureDirRecursive(const std::string& path){
    if(path.empty()){
        return false;
    }
    std::string current;
    size_t pos = 0;
    if(path[0] == '/'){
        current = "/";
        pos = 1;
    }
    while(pos <= path.size()){
        const size_t next = path.find('/', pos);
        const size_t len = (next == std::string::npos) ? (path.size() - pos) : (next - pos);
        const std::string part = path.substr(pos, len);
        if(!part.empty()){
            if(!current.empty() && current.back() != '/'){
                current.push_back('/');
            }
            current += part;
            if(!ensureDir(current)){
                return false;
            }
        }
        if(next == std::string::npos){
            break;
        }
        pos = next + 1;
    }
    return true;
}

std::string sanitizeIdToken(const std::string& in){
    std::string out;
    out.reserve(in.size());
    for(char c : in){
        const unsigned char u = static_cast<unsigned char>(c);
        if(std::isalnum(u)){
            out.push_back(static_cast<char>(std::tolower(u)));
        } else if(c == '-' || c == '_' || std::isspace(u)){
            out.push_back('_');
        }
    }
    // collapse consecutive underscores
    std::string collapsed;
    collapsed.reserve(out.size());
    bool prevUnderscore = false;
    for(char c : out){
        if(c == '_'){
            if(!prevUnderscore){
                collapsed.push_back(c);
            }
            prevUnderscore = true;
        } else {
            collapsed.push_back(c);
            prevUnderscore = false;
        }
    }
    if(!collapsed.empty() && collapsed.front() == '_'){
        collapsed.erase(collapsed.begin());
    }
    while(!collapsed.empty() && collapsed.back() == '_'){
        collapsed.pop_back();
    }
    if(collapsed.empty()){
        return "drum_set";
    }
    return collapsed;
}

InstrumentDefaults makeDefaultInstrumentDefaults(){
    InstrumentDefaults defaults;
    defaults.voice.attack = 0.0f;
    defaults.voice.decay = 0.0f;
    defaults.voice.sustain = 1.0f;
    defaults.voice.release = 0.08f;
    defaults.voice.repeat = false;
    defaults.effects.clear();
    return defaults;
}

void moveBestMatchToFront(std::vector<InstrumentInfo>& catalog, const std::string& preferredNeedle){
    if(catalog.empty()){
        return;
    }
    const std::string needle = toLowerCopy(preferredNeedle);
    auto it = std::find_if(catalog.begin(), catalog.end(), [&](const InstrumentInfo& info){
        return toLowerCopy(info.displayName).find(needle) != std::string::npos;
    });
    if(it != catalog.end()){
        std::swap(catalog.front(), *it);
    }
}

struct DrumSetPieceDefinition {
    std::string role;
    std::string instrumentId;
    std::string sampleFilter;
};

struct DrumSetDefinition {
    std::string id;
    std::string displayName;
    std::vector<DrumSetPieceDefinition> pieces;
};

int midiNoteForDrumRole(const std::string& roleRaw){
    const std::string role = toLowerCopy(trimCopy(roleRaw));
    static const std::unordered_map<std::string, int> roleMap = {
        {"kick", 36},
        {"bass", 36},
        {"bd", 36},
        {"side_stick", 37},
        {"rim", 37},
        {"snare", 38},
        {"sd", 38},
        {"clap", 39},
        {"closed_hihat", 42},
        {"closed_hat", 42},
        {"chh", 42},
        {"pedal_hihat", 44},
        {"pedal_hat", 44},
        {"phh", 44},
        {"low_tom", 45},
        {"tom_low", 45},
        {"open_hihat", 46},
        {"open_hat", 46},
        {"ohh", 46},
        {"mid_tom", 47},
        {"tom_mid", 47},
        {"high_tom", 50},
        {"tom_high", 50},
        {"crash", 49},
        {"crash_cymbal", 49},
        {"ride", 51},
        {"ride_cymbal", 51},
        {"ride_bell", 53},
        {"tambourine", 54},
        {"cowbell", 56},
        {"shaker", 82},
        {"triangle_closed", 80},
        {"closed_triangle", 80},
        {"triangle_open", 81},
        {"open_triangle", 81}
    };
    auto it = roleMap.find(role);
    if(it == roleMap.end()){
        return -1;
    }
    return it->second;
}

std::vector<DrumSetDefinition> loadDrumSetDefinitions(const std::string& path){
    std::vector<DrumSetDefinition> sets;
    std::ifstream file(path);
    if(!file){
        return sets;
    }

    std::string line;
    DrumSetDefinition* currentSet = nullptr;
    while(std::getline(file, line)){
        const std::string trimmed = trimCopy(line);
        if(trimmed.empty() || trimmed[0] == '#'){
            continue;
        }
        const std::vector<std::string> tokens = splitTabRow(trimmed);
        if(tokens.empty()){
            continue;
        }
        const std::string tag = toLowerCopy(trimCopy(tokens[0]));
        if(tag == "set"){
            std::string setId;
            std::string displayName;
            if(tokens.size() >= 2){
                setId = sanitizeIdToken(trimCopy(tokens[1]));
            }
            if(tokens.size() >= 3){
                displayName = trimCopy(tokens[2]);
            }
            if(displayName.empty()){
                displayName = setId.empty() ? "Drum Set" : setId;
            }
            if(setId.empty()){
                setId = sanitizeIdToken(displayName);
            }
            sets.push_back({setId, displayName, {}});
            currentSet = &sets.back();
            continue;
        }
        if((tag == "piece" || tag == "slot") && currentSet != nullptr && tokens.size() >= 3){
            std::string role = toLowerCopy(trimCopy(tokens[1]));
            std::string instrumentId = trimCopy(tokens[2]);
            std::string sampleFilter;
            if(tokens.size() >= 4){
                sampleFilter = trimCopy(tokens[3]);
            }
            if(role.empty() || instrumentId.empty()){
                continue;
            }
            auto existing = std::find_if(currentSet->pieces.begin(), currentSet->pieces.end(),
                [&](const DrumSetPieceDefinition& p){ return p.role == role; });
            if(existing != currentSet->pieces.end()){
                existing->instrumentId = instrumentId;
                existing->sampleFilter = sampleFilter;
            } else {
                currentSet->pieces.push_back({role, instrumentId, sampleFilter});
            }
        }
    }

    sets.erase(std::remove_if(sets.begin(), sets.end(), [](const DrumSetDefinition& set){
        return set.pieces.empty();
    }), sets.end());
    return sets;
}

std::unordered_map<std::string, std::string> loadInstrumentZoneLookup(const std::string& indexPath){
    std::unordered_map<std::string, std::string> lookup;
    std::ifstream indexFile(indexPath);
    if(!indexFile){
        return lookup;
    }
    std::string line;
    if(!std::getline(indexFile, line)){
        return lookup;
    }
    const std::vector<std::string> header = splitTabRow(line);
    const int instrumentIdIndex = findColumnIndex(header, "instrument_id");
    const int zonesFileIndex = findColumnIndex(header, "zones_file");
    if(instrumentIdIndex < 0 || zonesFileIndex < 0){
        return lookup;
    }

    while(std::getline(indexFile, line)){
        if(trimCopy(line).empty()){
            continue;
        }
        const std::vector<std::string> row = splitTabRow(line);
        if(static_cast<int>(row.size()) <= std::max(instrumentIdIndex, zonesFileIndex)){
            continue;
        }
        const std::string instrumentId = trimCopy(row[static_cast<size_t>(instrumentIdIndex)]);
        const std::string zonesFile = trimCopy(row[static_cast<size_t>(zonesFileIndex)]);
        if(instrumentId.empty() || zonesFile.empty()){
            continue;
        }
        lookup[instrumentId] = zonesFile;
    }
    return lookup;
}

std::string getCell(const std::vector<std::string>& row, int index){
    if(index < 0){
        return "";
    }
    const size_t safeIndex = static_cast<size_t>(index);
    if(safeIndex >= row.size()){
        return "";
    }
    return trimCopy(row[safeIndex]);
}

bool appendRemappedZonesFromSource(const std::string& sourceZonePath,
                                   int targetMidiNote,
                                   int& nextZoneId,
                                   std::vector<std::string>& outputRows,
                                   const std::string& sampleFilter = ""){
    std::ifstream zoneFile(sourceZonePath);
    if(!zoneFile){
        std::printf("DrumCatalog: cannot open source zones file: %s\n", sourceZonePath.c_str());
        return false;
    }

    std::string line;
    if(!std::getline(zoneFile, line)){
        return false;
    }
    const std::vector<std::string> header = splitTabRow(line);
    const int sampleIndex = findColumnIndex(header, "sample_relpath");
    if(sampleIndex < 0){
        std::printf("DrumCatalog: source zones missing sample_relpath: %s\n", sourceZonePath.c_str());
        return false;
    }
    const int lovelIndex = findColumnIndex(header, "lovel");
    const int hivelIndex = findColumnIndex(header, "hivel");
    const int tuneIndex = findColumnIndex(header, "tune_cents");
    const int volumeIndex = findColumnIndex(header, "volume_db");
    const int triggerIndex = findColumnIndex(header, "trigger");
    const int offsetIndex = findColumnIndex(header, "offset");
    const int ampVeltrackIndex = findColumnIndex(header, "amp_veltrack");
    const std::string filterNeedle = toLowerCopy(trimCopy(sampleFilter));

    bool addedAny = false;
    while(std::getline(zoneFile, line)){
        if(trimCopy(line).empty()){
            continue;
        }
        const std::vector<std::string> row = splitTabRow(line);
        const std::string sampleRelPath = getCell(row, sampleIndex);
        if(sampleRelPath.empty()){
            continue;
        }
        if(!filterNeedle.empty() && toLowerCopy(sampleRelPath).find(filterNeedle) == std::string::npos){
            continue;
        }
        std::string lovel = getCell(row, lovelIndex);
        std::string hivel = getCell(row, hivelIndex);
        std::string tune = getCell(row, tuneIndex);
        std::string volumeDb = getCell(row, volumeIndex);
        std::string trigger = toLowerCopy(getCell(row, triggerIndex));
        std::string offset = getCell(row, offsetIndex);
        std::string ampVeltrack = getCell(row, ampVeltrackIndex);

        if(lovel.empty()) lovel = "0";
        if(hivel.empty()) hivel = "127";
        if(tune.empty()) tune = "0.0";
        if(volumeDb.empty()) volumeDb = "0.0";
        if(trigger.empty()) trigger = "attack";
        if(offset.empty()) offset = "0";
        if(ampVeltrack.empty()) ampVeltrack = "100.0";

        std::ostringstream outRow;
        outRow << nextZoneId++ << '\t'
               << sampleRelPath << '\t'
               << targetMidiNote << '\t'
               << targetMidiNote << '\t'
               << lovel << '\t'
               << hivel << '\t'
               << targetMidiNote << '\t'
               << tune << '\t'
               << volumeDb << '\t'
               << trigger << '\t'
               << offset << '\t'
               << ampVeltrack;
        outputRows.push_back(outRow.str());
        addedAny = true;
    }
    return addedAny;
}

bool compileDrumSetToZonesFile(const DrumSetDefinition& setDef,
                               const std::unordered_map<std::string, std::string>& zoneLookup,
                               const std::string& tableRoot,
                               const std::string& generatedDir,
                               std::string& outZonesPath){
    if(setDef.pieces.empty()){
        return false;
    }
    if(!ensureDirRecursive(generatedDir)){
        std::printf("DrumCatalog: could not create generated dir: %s\n", generatedDir.c_str());
        return false;
    }

    const std::string safeId = sanitizeIdToken(setDef.id);
    outZonesPath = generatedDir + "/" + safeId + ".zones.tsv";
    std::vector<std::string> outputRows;
    int nextZoneId = 1;

    for(const auto& piece : setDef.pieces){
        const int midiNote = midiNoteForDrumRole(piece.role);
        if(midiNote < 0){
            std::printf("DrumCatalog: unknown drum role '%s' in set '%s'\n",
                piece.role.c_str(), setDef.displayName.c_str());
            continue;
        }
        auto zoneIt = zoneLookup.find(piece.instrumentId);
        if(zoneIt == zoneLookup.end()){
            std::printf("DrumCatalog: instrument_id '%s' not found for set '%s'\n",
                piece.instrumentId.c_str(), setDef.displayName.c_str());
            continue;
        }
        const std::string sourceZonePath = tableRoot + "/" + zoneIt->second;
        const bool added = appendRemappedZonesFromSource(
            sourceZonePath,
            midiNote,
            nextZoneId,
            outputRows,
            piece.sampleFilter
        );
        if(!added && !piece.sampleFilter.empty()){
            std::printf("DrumCatalog: no zones matched filter '%s' for instrument '%s' in set '%s'\n",
                piece.sampleFilter.c_str(), piece.instrumentId.c_str(), setDef.displayName.c_str());
        }
    }

    if(outputRows.empty()){
        return false;
    }

    std::ofstream outFile(outZonesPath, std::ios::trunc);
    if(!outFile){
        std::printf("DrumCatalog: failed to write generated set file: %s\n", outZonesPath.c_str());
        return false;
    }
    outFile << "zone_id\tsample_relpath\tlokey\thikey\tlovel\thivel\tpitch_keycenter\ttune_cents\tvolume_db\ttrigger\toffset\tamp_veltrack\n";
    for(const auto& row : outputRows){
        outFile << row << "\n";
    }
    return true;
}
}

InstrumentCatalog::InstrumentCatalog(){
    loadFromVCSLTables();
}

DrumCatalog::DrumCatalog(){
    loadFromVCSLTables();
}

void InstrumentCatalog::loadFromVCSLTables(){
    catalog.clear();
    const std::string tableRoot = resolveExistingPath("/root/Bela/Samples/bela_tables/vcsl_full", "Samples/bela_tables/vcsl_full");
    const std::string indexPath = tableRoot + "/instruments.tsv";
    std::ifstream indexFile(indexPath);
    if(!indexFile){
        catalog.push_back({"Grand Piano, Kawai", "chordophones_zithers_grand_piano_kawai", makeDefaultInstrumentDefaults()});
        return;
    }

    std::string line;
    if(!std::getline(indexFile, line)){
        catalog.push_back({"Grand Piano, Kawai", "chordophones_zithers_grand_piano_kawai", makeDefaultInstrumentDefaults()});
        return;
    }

    const std::vector<std::string> header = splitTabRow(line);
    const int instrumentIdIndex = findColumnIndex(header, "instrument_id");
    const int displayNameIndex = findColumnIndex(header, "display_name");
    const int zoneCountIndex = findColumnIndex(header, "zone_count");
    if(instrumentIdIndex < 0 || displayNameIndex < 0){
        catalog.push_back({"Grand Piano, Kawai", "chordophones_zithers_grand_piano_kawai", makeDefaultInstrumentDefaults()});
        return;
    }

    while(std::getline(indexFile, line)){
        if(trimCopy(line).empty()){
            continue;
        }
        const std::vector<std::string> row = splitTabRow(line);
        if(static_cast<int>(row.size()) <= std::max(instrumentIdIndex, displayNameIndex)){
            continue;
        }
        const std::string instrumentId = trimCopy(row[static_cast<size_t>(instrumentIdIndex)]);
        const std::string displayName = trimCopy(row[static_cast<size_t>(displayNameIndex)]);
        int zoneCount = 1;
        if(zoneCountIndex >= 0 && static_cast<int>(row.size()) > zoneCountIndex){
            parseInt(row[static_cast<size_t>(zoneCountIndex)], zoneCount);
        }
        if(instrumentId.empty() || displayName.empty()){
            continue;
        }
        if(zoneCount <= 0){
            continue;
        }
        if(toLowerCopy(displayName).find("keyswitch") != std::string::npos){
            continue;
        }
        if(toLowerCopy(displayName).find("legacy") != std::string::npos){
            continue;
        }
        catalog.push_back({displayName, instrumentId, makeDefaultInstrumentDefaults()});
    }

    if(catalog.empty()){
        catalog.push_back({"Grand Piano, Kawai", "chordophones_zithers_grand_piano_kawai", makeDefaultInstrumentDefaults()});
    } else {
        moveBestMatchToFront(catalog, "Grand Piano");
    }
}

void DrumCatalog::loadFromVCSLTables(){
    catalog.clear();
    const std::string tableRoot = resolveExistingPath("/root/Bela/Samples/bela_tables/vcsl_full", "Samples/bela_tables/vcsl_full");
    const std::string indexPath = tableRoot + "/instruments.tsv";
    std::ifstream indexFile(indexPath);
    if(!indexFile){
        catalog.push_back({"Snare Drum, Modern 1", "membranophones_struck_membranophones_snare_drum_modern_1", makeDefaultInstrumentDefaults()});
        return;
    }

    std::string line;
    if(!std::getline(indexFile, line)){
        catalog.push_back({"Snare Drum, Modern 1", "membranophones_struck_membranophones_snare_drum_modern_1", makeDefaultInstrumentDefaults()});
        return;
    }

    const std::vector<std::string> header = splitTabRow(line);
    const int instrumentIdIndex = findColumnIndex(header, "instrument_id");
    const int displayNameIndex = findColumnIndex(header, "display_name");
    const int categoryIndex = findColumnIndex(header, "category");
    const int zoneCountIndex = findColumnIndex(header, "zone_count");
    if(instrumentIdIndex < 0 || displayNameIndex < 0 || categoryIndex < 0){
        catalog.push_back({"Snare Drum, Modern 1", "membranophones_struck_membranophones_snare_drum_modern_1", makeDefaultInstrumentDefaults()});
        return;
    }

    while(std::getline(indexFile, line)){
        if(trimCopy(line).empty()){
            continue;
        }
        const std::vector<std::string> row = splitTabRow(line);
        if(static_cast<int>(row.size()) <= std::max(categoryIndex, std::max(instrumentIdIndex, displayNameIndex))){
            continue;
        }
        const std::string instrumentId = trimCopy(row[static_cast<size_t>(instrumentIdIndex)]);
        const std::string displayName = trimCopy(row[static_cast<size_t>(displayNameIndex)]);
        const std::string category = toLowerCopy(trimCopy(row[static_cast<size_t>(categoryIndex)]));
        int zoneCount = 1;
        if(zoneCountIndex >= 0 && static_cast<int>(row.size()) > zoneCountIndex){
            parseInt(row[static_cast<size_t>(zoneCountIndex)], zoneCount);
        }
        if(instrumentId.empty() || displayName.empty()){
            continue;
        }
        if(zoneCount <= 0){
            continue;
        }
        const bool looksDrumLike =
            category.find("membranophones") != std::string::npos ||
            category.find("idiophones") != std::string::npos;
        if(!looksDrumLike){
            continue;
        }
        if(toLowerCopy(displayName).find("keyswitch") != std::string::npos){
            continue;
        }
        if(toLowerCopy(displayName).find("legacy") != std::string::npos){
            continue;
        }
        catalog.push_back({displayName, instrumentId, makeDefaultInstrumentDefaults()});
    }

    if(catalog.empty()){
        catalog.push_back({"Snare Drum, Modern 1", "membranophones_struck_membranophones_snare_drum_modern_1", makeDefaultInstrumentDefaults()});
    } else {
        moveBestMatchToFront(catalog, "Snare");
    }

    const std::string dataRoot = resolveExistingPath("/root/Bela/Samples/data", "Samples/data");
    const std::string setDefinitionsPath = dataRoot + "/drum_sets.txt";
    const std::string generatedDir = dataRoot + "/drumsets/generated";
    const std::vector<DrumSetDefinition> drumSets = loadDrumSetDefinitions(setDefinitionsPath);
    if(!drumSets.empty()){
        const std::unordered_map<std::string, std::string> zoneLookup = loadInstrumentZoneLookup(indexPath);
        std::vector<InstrumentInfo> generatedSetEntries;
        generatedSetEntries.reserve(drumSets.size());
        for(const auto& setDef : drumSets){
            std::string generatedZonesPath;
            if(compileDrumSetToZonesFile(setDef, zoneLookup, tableRoot, generatedDir, generatedZonesPath)){
                generatedSetEntries.push_back({setDef.displayName, generatedZonesPath, makeDefaultInstrumentDefaults()});
            }
        }
        if(!generatedSetEntries.empty()){
            catalog.insert(catalog.begin(), generatedSetEntries.begin(), generatedSetEntries.end());
        }
    }
}

std::string& InstrumentCatalog::getDisplayName(size_t index){
    return VEC_AT(catalog, index).displayName;
}

std::string& InstrumentCatalog::getFolderName(size_t index){
    return VEC_AT(catalog, index).folderName;
}

InstrumentDefaults& InstrumentCatalog::getDefaults(size_t index){
    return VEC_AT(catalog, index).defaults;
}

std::string& DrumCatalog::getDisplayName(size_t index){
    return VEC_AT(catalog, index).displayName;
}

std::string& DrumCatalog::getFolderName(size_t index){
    return VEC_AT(catalog, index).folderName;
}

InstrumentDefaults& DrumCatalog::getDefaults(size_t index){
    return VEC_AT(catalog, index).defaults;
}
