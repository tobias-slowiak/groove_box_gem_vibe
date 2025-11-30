#include <Bela.h>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <cassert>

#include "../include/IDisplayContext.h"
#include "../include/DisplayContextReal.h"
#include "../include/ResourceManager.h"
#include "../include/DebugLog.h"
#include "../include/TaskWrapper.h"



//we need to init the u8g2s before initing the display context because of constructor order shananigans
std::vector<U8G2*> initU8G2s(){
	int i2cBus = 1;
	U8G2* u8g2_1 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, i2cBus, 0x3d);
	U8G2* u8g2_2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, i2cBus, 0x3c);
	std::vector<U8G2*> u8g2s{u8g2_1, u8g2_2};
	for(int display = 0; display < 2; display++){
		assert(u8g2s.size() > static_cast<size_t>(display));
		U8G2* u8g2 = u8g2s.at(display);
		u8g2->initDisplay();
		u8g2->setPowerSave(0);
		u8g2->setFont(u8g2_font_9x15B_tf);
		u8g2->setFontRefHeightText();
		u8g2->setFontPosTop();
		u8g2->clearBuffer();
	    int LINE_HEIGHT = 15;
	    if(display == 0)
	    	u8g2->drawStr(0, 0 * LINE_HEIGHT, "init left");
	    if(display == 1)
	    	u8g2->drawStr(0, 0 * LINE_HEIGHT, "init right");
	    u8g2->sendBuffer();
	}
	printf("done initializing the u8g2s\n");
	return u8g2s;
}

//////////////////////////////////MEMBER FUNCTIONS


DisplayContextReal::DisplayContextReal(ResourceManager* resourceManager, std::vector<U8G2*> u8g2s)
		: resourceManager(resourceManager), u8g2s(u8g2s),
		lines(std::vector<std::vector<std::string>>{std::vector<std::string>(NUM_LINES,""), std::vector<std::string>(NUM_LINES,"")}),
		displayTask(this, 50, "displayTask"){
	assert(resourceManager != nullptr);
}

void DisplayContextReal::initDisplayContext() {
	assert(resourceManager != nullptr);
	rt_printf("initializing real displaycontext\n");
	for(auto* u8g2: u8g2s){
		u8g2->setPowerSave(0);
		u8g2->setFont(u8g2_font_9x15B_tf);
		u8g2->setFontRefHeightText();
		u8g2->setFontPosTop();
		this->setLines(0,0,"Hi! :)","Let me", "brush up", "here");
		this->setLines(1,0, "real quick", "Thank you! :)");
	}
	displayTask.processBlockwise();
}

void DisplayContextReal::processBlockwise() {
	displayTask.processBlockwise();
}

void DisplayContextReal::setLines(int displayNumber, int lineNumber, std::string line0, std::string line1, std::string line2, std::string line3) {
	assert(resourceManager != nullptr);
	assert(displayNumber >= 0 && lines.size() > static_cast<size_t>(displayNumber));
	auto& displayLines = lines.at(displayNumber);
	assert(lineNumber >= 0 && displayLines.size() > static_cast<size_t>(lineNumber));
	displayLines.at(lineNumber) = line0;
	if(line1 != ""){
		assert(displayLines.size() > static_cast<size_t>(lineNumber + 1));
		displayLines.at(lineNumber + 1) = line1;
	}
	if(line2 != ""){
		assert(displayLines.size() > static_cast<size_t>(lineNumber + 2));
		displayLines.at(lineNumber + 2) = line2;
	}
	if(line3 != ""){
		assert(displayLines.size() > static_cast<size_t>(lineNumber + 3));
		displayLines.at(lineNumber + 3) = line3;
	}
	setScheduleFlag("displayTask");
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines){
	this->lines = lines;
	setScheduleFlag("displayTask");
}

std::string DisplayContextReal::getLine(int displayNumber, int lineNumber) {
	assert(displayNumber >= 0 && lines.size() > static_cast<size_t>(displayNumber));
	auto& displayLines = lines.at(displayNumber);
	assert(lineNumber >= 0 && displayLines.size() > static_cast<size_t>(lineNumber));
	return displayLines.at(lineNumber);
}

void DisplayContextReal::setProgress(int displayNumber, float percentage) {
	assert(resourceManager != nullptr);
	progress = percentage;
	progressDisplay = displayNumber;
	if(progress < 0.0){
		//code to erase progress bar;
		progressDisplay = -1;
	}
	setScheduleFlag("displayTask");
}

TaskWrapper& DisplayContextReal::getDisplayTask() {
	return displayTask;
}

void DisplayContextReal::renderDisplay() {
	assert(resourceManager != nullptr);
	for(int display = 0; display < 2; display++){
	    assert(u8g2s.size() > static_cast<size_t>(display));
	    U8G2* u8g2 = u8g2s.at(display);
	    u8g2->clearBuffer();
	    // Draw text lines
	    assert(lines.size() > static_cast<size_t>(display));
	    auto& displayLines = lines.at(display);
	    for (int line = 0; line < NUM_LINES; ++line) {
	        assert(displayLines.size() > static_cast<size_t>(line));
	        u8g2->drawStr(0, line * LINE_HEIGHT, displayLines.at(line).c_str());
	    }
	    if(progressDisplay == display){
		    int barWidth = static_cast<int>(progress * (SCREEN_WIDTH - 2));
		    int barY = SCREEN_HEIGHT - PROGRESS_HEIGHT;
		    u8g2->drawFrame(0, barY, SCREEN_WIDTH - 2, PROGRESS_HEIGHT);
		    u8g2->drawBox(0, barY, barWidth, PROGRESS_HEIGHT);
	    }
	    u8g2->sendBuffer();
	}
}

void DisplayContextReal::setScheduleFlag(std::string& taskName){
	if(taskName == "displayTask"){
		displayTask.setScheduleFlag();
	}
}

void DisplayContextReal::taskJob(const std::string& taskName){
	if(taskName == "displayTask"){
		renderDisplay();
		return;
	}
	throw std::runtime_error("DisplayContextReal::taskJob invoked with task name " + std::to_string(taskName))
}
