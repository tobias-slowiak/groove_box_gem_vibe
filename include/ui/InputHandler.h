#pragma once

class ResourceManager;
class UI;
class DeviceMap;
class InterfaceMessage;
class IMidiChannelMessage;
/*
TODO: make this device dependent e.g. via compile-time defines or via device-specific configuration after device detection
*/

class InputHandler {
public:
    InputHandler(ResourceManager& rm);
    void parseMessages();
    void handleMessage(InterfaceMessage& message);
    void handleMessage(IMidiChannelMessage& message);
private:
    ResourceManager& rm;
    UI& ui;
    DeviceMap& deviceMap;
};
