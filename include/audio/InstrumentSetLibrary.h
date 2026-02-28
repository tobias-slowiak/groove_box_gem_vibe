#pragma once

#include <string>
#include <vector>

struct InstrumentSetEntry {
    std::string instrumentId;
    std::string displayName;
};

struct InstrumentSet {
    std::string name;
    std::vector<InstrumentSetEntry> entries;
};

class InstrumentSetLibrary {
public:
    InstrumentSetLibrary();

    int getSetCount() const;
    int clampSetIndex(int index) const;
    int clampEntryIndex(int setIndex, int entryIndex) const;

    int getActiveSetIndex() const;
    void setActiveSetIndex(int index);

    const std::string& getSetName(int setIndex) const;
    void renameSet(int setIndex, const std::string& newName);

    int getEntryCount(int setIndex) const;
    const InstrumentSetEntry& getEntry(int setIndex, int entryIndex) const;

    bool addOrUpdateEntry(int setIndex, const std::string& instrumentId, const std::string& displayName);
    bool removeEntryByInstrumentId(int setIndex, const std::string& instrumentId);

    void addSet(const std::string& name);
    void removeSet(int setIndex);

private:
    void loadFromFile();
    void saveToFile() const;
    void ensureAtLeastOneSet();
    std::string sanitizeToken(const std::string& in) const;
    std::string trimCopy(const std::string& in) const;

    std::string storagePath;
    std::vector<InstrumentSet> sets;
    int activeSetIndex = 0;
};

