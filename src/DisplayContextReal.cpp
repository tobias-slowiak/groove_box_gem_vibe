#include <Bela.h>
#include <atomic>
#include <vector>
#include <stdexcept>

#include "../include/IDisplayContext.h"
#include "../include/DisplayContextReal.h"

///////////////////////////////////////////NONMEMBER-FUNCTIONS

void displayThreadFunction(void* arg) {
	IDisplayContext* displayContext = static_cast<IDisplayContext*>(arg);
	std::atomic<bool>& updateDisplayFlag = displayContext->getUpdateDisplayFlag();
    if (updateDisplayFlag) {
        displayContext->renderDisplay();
    }
}




//we need to init the u8g2s before initing the display context because of constructor order shananigans
std::vector<U8G2*> initU8G2s(){
	int i2cBus = 1;
	U8G2* u8g2_1 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, i2cBus, 0x3d);
	U8G2* u8g2_2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, i2cBus, 0x3c);
	std::vector<U8G2*> u8g2s{u8g2_1, u8g2_2};
	for(int display = 0; display < 2; display++){
		u8g2s.at(display)->initDisplay();
		u8g2s.at(display)->setPowerSave(0);
		u8g2s.at(display)->setFont(u8g2_font_9x15B_tf);
		u8g2s.at(display)->setFontRefHeightText();
		u8g2s.at(display)->setFontPosTop();
		u8g2s.at(display)->clearBuffer();
	    int LINE_HEIGHT = 15;
	    if(display == 0)
	    	u8g2s.at(display)->drawStr(0, 0 * LINE_HEIGHT, "init left");
	    if(display == 1)
	    	u8g2s.at(display)->drawStr(0, 0 * LINE_HEIGHT, "init right");
	    u8g2s.at(display)->sendBuffer();
	}
	printf("done initializing the u8g2s\n");
	return u8g2s;
}

//////////////////////////////////MEMBER FUNCTIONS


DisplayContextReal::DisplayContextReal(std::vector<U8G2*> u8g2s, std::atomic<bool>& updateDisplayFlag): u8g2s(u8g2s), updateDisplayFlag(updateDisplayFlag) {
	lines = std::vector<std::vector<std::string>>{std::vector<std::string>(NUM_LINES,""), std::vector<std::string>(NUM_LINES,"")};
	displayTask = Bela_createAuxiliaryTask(displayThreadFunction, 50, "displayTask", (void*)this);
}

void DisplayContextReal::initDisplayContext() {
	rt_printf("initializing real displaycontext\n");
	for(auto* u8g2: u8g2s){
		u8g2->setPowerSave(0);
		u8g2->setFont(u8g2_font_9x15B_tf);
		u8g2->setFontRefHeightText();
		u8g2->setFontPosTop();
		this->setLines(0,0,"Hi! :)","Let me", "brush up", "here");
		this->setLines(1,0, "real quick", "Thank you! :)");
	}
	this->renderDisplay();
}

void DisplayContextReal::processBlockwise() {
	if(updateDisplayFlag) Bela_scheduleAuxiliaryTask(displayTask);
}

void DisplayContextReal::setLines(int displayNumber, int lineNumber, std::string line0, std::string line1, std::string line2, std::string line3) {
	updateDisplayFlag = true;
	lines.at(displayNumber).at(lineNumber) = line0;
	if(line1 != "") lines.at(displayNumber).at(lineNumber + 1) = line1;
	if(line2 != "") lines.at(displayNumber).at(lineNumber + 2) = line2;
	if(line3 != "") lines.at(displayNumber).at(lineNumber + 3) = line3;
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines){
	this->lines = lines;
	updateDisplayFlag = true;
}

std::string DisplayContextReal::getLine(int displayNumber, int lineNumber) {
	return lines.at(displayNumber).at(lineNumber);
}

void DisplayContextReal::setProgress(int displayNumber, float percentage) {
	updateDisplayFlag = true;
	progress = percentage;
	progressDisplay = displayNumber;
}

std::atomic<bool>& DisplayContextReal::getUpdateDisplayFlag() {
	return updateDisplayFlag;
}
	
AuxiliaryTask& DisplayContextReal::getDisplayTask() {
	return displayTask;
}

void DisplayContextReal::renderDisplay() {
	updateDisplayFlag = false;
	for(int display = 0; display < 2; display++){
	    u8g2s.at(display)->clearBuffer();
	    // Draw text lines
	    for (int line = 0; line < NUM_LINES; ++line) {
	        u8g2s.at(display)->drawStr(0, line * LINE_HEIGHT, lines.at(display).at(line).c_str());
	    }
	    if(progressDisplay == display){
		    int barWidth = static_cast<int>(progress * (SCREEN_WIDTH - 2));
		    int barY = SCREEN_HEIGHT - PROGRESS_HEIGHT;
		    u8g2s.at(display)->drawFrame(0, barY, SCREEN_WIDTH - 2, PROGRESS_HEIGHT);
		    u8g2s.at(display)->drawBox(0, barY, barWidth, PROGRESS_HEIGHT);
	    }
	    u8g2s.at(display)->sendBuffer();
	}

}