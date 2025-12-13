#pragma once

#include <Bela.h>
#include "u8g2/U8g2LinuxI2C.h"
#include <atomic>
#include <vector>
#include <stdexcept>

#include "IDisplayContext.h"
class ResourceManager;
#include "../include/TaskWrapper.h"
#include "../include/DisplayMessage.h"
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

    void sendTaskMessage();

	std::string getLine(int displayNumber, int lineNumber) override;
	
	void setProgress(int displayNumber, float percentage) override;
    
    ResourceManager* getResourceManager() { return resourceManager; }
		
	void renderDisplay() override;

    void taskWorkMessage(std::string& taskName, DisplayMessage msg);
	
private:

    ResourceManager* resourceManager;

    int NUM_LINES = 4;
    int LINE_HEIGHT = 15;
    int SCREEN_WIDTH = 128;
    int SCREEN_HEIGHT = 64;
    int PROGRESS_HEIGHT = 8;


	std::vector<std::vector<std::string>> lines;
	std::vector<U8G2*> u8g2s;
    
    TaskWrapper<DisplayContextReal, DisplayMessage> renderTask;
    
    int progressDisplay = -1; //determines which display shows the progress bar
    float progress = 0.0f;

    //render Task only
    std::vector<std::vector<std::string>> r_lines;
    int r_progressDisplay = -1; 
    float r_progress = 0.0f;
};
