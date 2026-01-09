#include "../../include/audio/InstrumentCatalog.h"
#include "../../include/general/BasicUtilities.h"
//compiel
std::string& InstrumentCatalog::getDisplayName(size_t index){
    return VEC_AT(catalog, index).displayName;
}

std::string& InstrumentCatalog::getFolderName(size_t index){
    return VEC_AT(catalog, index).folderName;
}

std::string& DrumCatalog::getDisplayName(size_t index){
    return VEC_AT(catalog, index).displayName;
}

std::string& DrumCatalog::getFolderName(size_t index){
    return VEC_AT(catalog, index).folderName;
}