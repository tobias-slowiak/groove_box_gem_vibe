#pragma once

class ResourceManager;
class Loopers;
class DeviceMap;
class IMidi;

enum class LooperLightMessage {
    PlayingOn,
    PlayingOff,
    RecordingOn,
    RecordingOff,
    WaitingForBarStart
};

class LooperLights {
public:
    LooperLights(ResourceManager& resourceManager);

    void initialize();

    void setLight(LooperLightMessage msg, int looperIndex);
private:
    ResourceManager& resourceManager;
    DeviceMap& deviceMap;
    IMidi& midi;
};