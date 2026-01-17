#pragma once

class ResourceManager;
class Loopers;
class DeviceMap;

class LooperLights {
public:
    LooperLights(ResourceManager& resourceManager);

    void initialize();

    void update();

    void turnOnPlayingLight(int looperIndex);
    void turnOffPlayingLight(int looperIndex);
    void turnOnRecordingLight(int looperIndex);
    void turnOffRecordingLight(int looperIndex);
    void turnOnWaitingForBarStartLight(int looperIndex);
    void turnOffWaitingForBarStartLight(int looperIndex);
private:
    ResourceManager& resourceManager;
    Loopers& loopers;
    DeviceMap& deviceMap;
};