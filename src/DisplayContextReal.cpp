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
		: resourceManager(resourceManager),
		lines(std::vector<std::vector<std::string>>{std::vector<std::string>(NUM_LINES,""), std::vector<std::string>(NUM_LINES,"")}),
		u8g2s(u8g2s),
		renderTask(this, 70, "renderTask"){
	assert(resourceManager != nullptr);
	r_lines = lines;
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
	renderDisplay();
}

void DisplayContextReal::processBlockwise() {
	renderTask.taskCheckAndWorkMessages();
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
	sendTaskMessage();
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines){
	this->lines = lines;
	sendTaskMessage();
}

void DisplayContextReal::sendTaskMessage(){
	DisplayMessage msg;
	msg.lines = lines;
	msg.progressDisplay = progressDisplay;
	msg.progress = progress;
	renderTask.pushMessage(TaskMessageTarget::TaskThread, msg);
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
	sendTaskMessage();
}


void DisplayContextReal::renderDisplay() {
	assert(resourceManager != nullptr);
	for(int display = 0; display < 2; display++){
	    assert(u8g2s.size() > static_cast<size_t>(display));
	    U8G2* u8g2 = u8g2s.at(display);
	    u8g2->clearBuffer();
	    // Draw text lines
	    assert(r_lines.size() > static_cast<size_t>(display));
	    auto& displayLines = r_lines.at(display);
	    for (int line = 0; line < NUM_LINES; ++line) {
	        assert(displayLines.size() > static_cast<size_t>(line));
	        u8g2->drawStr(0, line * LINE_HEIGHT, displayLines.at(line).c_str());
	    }
	    if(r_progressDisplay == display){
		    int barWidth = static_cast<int>(r_progress * (SCREEN_WIDTH - 2));
		    int barY = SCREEN_HEIGHT - PROGRESS_HEIGHT;
		    u8g2->drawFrame(0, barY, SCREEN_WIDTH - 2, PROGRESS_HEIGHT);
		    u8g2->drawBox(0, barY, barWidth, PROGRESS_HEIGHT);
	    }
	    u8g2->sendBuffer();
	}
}

void DisplayContextReal::taskWorkMessage(std::string& taskName, DisplayMessage msg){
	if(taskName == "renderTask"){
		r_lines = msg.lines;
		r_progressDisplay = msg.progressDisplay;
		r_progress = msg.progress;
		renderDisplay();
		return;
	}
	throw std::runtime_error("DisplayContextReal::taskWorkMessage invoked with task name " + taskName);
}