#pragma once

#include <Bela.h>
#include "u8g2/U8g2LinuxI2C.h"
#include <atomic>
#include <vector>
#include <stdexcept>

#include "IDisplayContext.h"
class ResourceManager;

///////////////////////////////////////////NONMEMBER-FUNCTIONS

void displayThreadFunction(void* arg);

std::vector<U8G2*> initU8G2s();

//////////////////////////////////////////////CLASS


class DisplayContextReal : public IDisplayContext {
public:
    DisplayContextReal(ResourceManager* resourceManager, std::vector<U8G2*> u8g2s);
    
    void initDisplayContext() override;
    
    void processBlockwise() override;
	
	void setLines(int displayNumber, int lineNumber, std::string line0, std::string line1 = "", std::string line2 = "", std::string line3 = "") override;
	
	void setLines(std::vector<std::vector<std::string>>) override;

	std::string getLine(int displayNumber, int lineNumber) override;
	
	void setProgress(int displayNumber, float percentage) override;
    
    ResourceManager* getResourceManager() { return resourceManager; }
	
	AuxiliaryTask& getDisplayTask() override;
	
	void renderDisplay() override;
	
private:
    friend void displayThreadFunction(void* arg);

    void requestDisplayUpdate();
    bool hasPendingDisplayUpdate() const;
    void drainPendingDisplayUpdates();
    void flushPendingUpdatesSync();

    ResourceManager* resourceManager;
	std::vector<std::vector<std::string>> lines;
	std::vector<U8G2*> u8g2s;
    AuxiliaryTask displayTask;
    std::atomic<uint32_t> displayUpdatesRequested{0};
    std::atomic<uint32_t> displayUpdatesRendered{0};
    std::atomic<bool> displayTaskInFlight{false};
    int progressDisplay = -1; //determines which display shows the progress bar
    float progress = 0.0f;
    int NUM_LINES = 4;
    int LINE_HEIGHT = 15;
    int SCREEN_WIDTH = 128;
    int SCREEN_HEIGHT = 64;
    int PROGRESS_HEIGHT = 8;
};
