#include<Bela.h>
#include <vector>
#include <map>
#include <utility>

class StreamingBuffer;


class SamplePack {
public:
    // make one for an instrument samplepack (larger) and one for a drum sample pack (smaller)
    SamplePack(ResourceManager* resourceManager, std::string samplePackName, std::string samplePackFolderName, size_t bigSamplePackBufferLength);

    
    
    ResourceManager* resourceManager;
    StreamingBuffer streamingBuffer;
};