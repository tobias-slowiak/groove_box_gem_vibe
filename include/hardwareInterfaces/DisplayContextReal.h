#pragma once

#include <Bela.h>
#include "u8g2/U8g2LinuxI2C.h"
#include <atomic>
#include <vector>
#include <stdexcept>
//compiel
#include "IDisplayContext.h"
class ResourceManager;
#include "../general/TaskWrapper.h"
#include "DisplayMessage.h"
///////////////////////////////////////////NONMEMBER-FUNCTIONS

void displayThreadFunction(void* arg);

std::vector<U8G2*> initU8G2s(int numLines);

//////////////////////////////////////////////CLASS


class DisplayContextReal : public IDisplayContext {
public:
    DisplayContextReal(ResourceManager& resourceManager, std::vector<U8G2*> u8g2s, int numLines);
    
    void initDisplayContext() override;
    
    void processBlockwise() override;
		
	void setLines(std::vector<std::vector<std::string>>) override;
    
    void setLines(std::vector<std::vector<std::string>> lines, std::vector<std::vector<TextFrame>> textFrames) override;
	
	void setLines(std::vector<std::vector<std::string>> lines,
				  std::vector<std::vector<TextFrame>> textFrames,
				  std::vector<ScrollBar> scrollBars) override;

    void sendTaskMessage();

	std::string getLine(int displayNumber, int lineNumber) override;
	
	void setProgress(int displayNumber, float percentage) override;
    
    ResourceManager& getResourceManager() { return resourceManager; }
		
	void renderDisplay() override;

    void taskWorkMessage(std::string& taskName, DisplayMessage msg);
	
private:

    ResourceManager& resourceManager;

    int SCREEN_WIDTH = 128;
    int SCREEN_HEIGHT = 64;
    int PROGRESS_HEIGHT = 8;


    const int NUM_LINES;
    const uint8_t* FONT;
    const int CHARACTER_HEIGHT;
    const int CHARACTER_WIDTH;


	std::vector<std::vector<std::string>> lines;
	std::vector<U8G2*> u8g2s;
    
    TaskWrapper<DisplayContextReal, DisplayMessage> renderTask;
    
    int progressDisplay = -1; //determines which display shows the progress bar
    float progress = 0.0f;

    std::vector<std::vector<TextFrame>> textFrames;
    std::vector<ScrollBar> scrollBars;

    //render Task only
    std::vector<std::vector<std::string>> r_lines;
    int r_progressDisplay = -1; 
    float r_progress = 0.0f;
    std::vector<std::vector<TextFrame>> r_textFrames;
    std::vector<ScrollBar> r_scrollBars;

    
};
