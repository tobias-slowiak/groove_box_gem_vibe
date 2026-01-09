#include <Bela.h>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <cassert>
//compiel
#include "../../include/hardwareInterfaces/IDisplayContext.h"
#include "../../include/hardwareInterfaces/DisplayContextReal.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/general/DebugLog.h"
#include "../../include/general/TaskWrapper.h"



//we need to init the u8g2s before initing the display context because of constructor order shananigans
std::vector<U8G2*> initU8G2s(int numLines){
	const uint8_t* FONT = (numLines == 3) ? u8g2_font_10x20_tf : (numLines == 4) ? u8g2_font_9x15B_tf : u8g2_font_6x12_tf;
	int i2cBus = 1;
	U8G2* u8g2_1 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, i2cBus, 0x3d);
	U8G2* u8g2_2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, i2cBus, 0x3c);
	std::vector<U8G2*> u8g2s{u8g2_1, u8g2_2};
	for(int display = 0; display < 2; display++){
		assert(u8g2s.size() > static_cast<size_t>(display));
		U8G2* u8g2 = u8g2s.at(display);
		u8g2->initDisplay();
		u8g2->setPowerSave(0);
		u8g2->setFont(FONT);
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


DisplayContextReal::DisplayContextReal(ResourceManager& resourceManager, std::vector<U8G2*> u8g2s,
										int numLines)
		: resourceManager(resourceManager),
		  NUM_LINES(numLines),
		  FONT((numLines == 3) ? u8g2_font_10x20_tf : (numLines == 4) ? u8g2_font_9x15B_tf : u8g2_font_6x12_tf),
		  CHARACTER_HEIGHT([numLines]{ if(numLines == 3) return 20; else if(numLines == 4) return 15; else return 12; }()),
		  CHARACTER_WIDTH([numLines]{ if(numLines == 3) return 10; else if(numLines == 4) return 9; else return 6; }()),
		  lines(std::vector<std::vector<std::string>>{std::vector<std::string>(NUM_LINES,""), std::vector<std::string>(NUM_LINES,"")}),
		  u8g2s(u8g2s),
		  renderTask(this, 70, "renderTask"){
	r_lines = lines;
	if(numLines < 3 || numLines > 5){
		throw std::runtime_error("DisplayContextReal: invalid display parameters");
	}
}

void DisplayContextReal::initDisplayContext() {
	rt_printf("initializing real displaycontext\n");
	for(auto* u8g2: u8g2s){
		u8g2->setPowerSave(0);
		u8g2->setFont(FONT);
		u8g2->setFontRefHeightText();
		u8g2->setFontPosTop();
		this->setLines({{"Hi", "there!"}, {"Display", "initialized"}});
	}
	renderDisplay();
}

void DisplayContextReal::processBlockwise() {
	renderTask.taskCheckAndWorkMessages();
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines){
	for(int displayNum = 0; displayNum < 2; displayNum++){
		for(int lineNum = 0; lineNum < NUM_LINES; lineNum++){
			if(displayNum < static_cast<int>(lines.size()) && lineNum < static_cast<int>(lines[displayNum].size())){
				this->lines[displayNum][lineNum] = lines[displayNum][lineNum];
			} else {
				this->lines[displayNum][lineNum] = "";
			}
		}
	}	
	this->textFrames.clear();
	sendTaskMessage();
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines, std::vector<std::vector<TextFrame>> textFrames){
	this->lines = lines;
	this->textFrames = textFrames;
	sendTaskMessage();
}

void DisplayContextReal::sendTaskMessage(){
	DisplayMessage msg;
	msg.lines = lines;
	msg.progressDisplay = progressDisplay;
	msg.progress = progress;
	msg.textFrames = textFrames;
	renderTask.pushMessage(TaskMessageTarget::TaskThread, msg);
}

std::string DisplayContextReal::getLine(int displayNumber, int lineNumber) {
	assert(displayNumber >= 0 && lines.size() > static_cast<size_t>(displayNumber));
	auto& displayLines = lines.at(displayNumber);
	assert(lineNumber >= 0 && displayLines.size() > static_cast<size_t>(lineNumber));
	return displayLines.at(lineNumber);
}

void DisplayContextReal::setProgress(int displayNumber, float percentage) {
	progress = percentage;
	progressDisplay = displayNumber;
	if(progress < 0.0){
		//code to erase progress bar;
		progressDisplay = -1;
	}
	sendTaskMessage();
}


void DisplayContextReal::renderDisplay() {
	for(int display = 0; display < 2; display++){
	    assert(u8g2s.size() > static_cast<size_t>(display));
	    U8G2* u8g2 = u8g2s.at(display);
	    if(r_textFrames.empty()){
	    	u8g2->clearDisplay();
	    }
	    u8g2->clearBuffer();
	    // Draw text lines
	    assert(r_lines.size() > static_cast<size_t>(display));
	    auto& displayLines = r_lines.at(display);
	    for (int line = 0; line < NUM_LINES; ++line) {
	        assert(displayLines.size() > static_cast<size_t>(line));
	        u8g2->drawStr(2, line * CHARACTER_HEIGHT, displayLines.at(line).c_str());
	    }
	    if(r_progressDisplay == display){
		    int barWidth = static_cast<int>(r_progress * (SCREEN_WIDTH - 2));
		    int barY = SCREEN_HEIGHT - PROGRESS_HEIGHT;
		    u8g2->drawFrame(0, barY, SCREEN_WIDTH - 2, PROGRESS_HEIGHT);
		    u8g2->drawBox(0, barY, barWidth, PROGRESS_HEIGHT);
	    }
		if(r_textFrames.size() > display){
			std::vector<TextFrame>& textFramesOnThisDisplay = VEC_AT(r_textFrames, display);
			for(auto frame: textFramesOnThisDisplay){
				int x = frame.startChar * CHARACTER_WIDTH; //approx char width
				int y = frame.textLine * CHARACTER_HEIGHT - 1;
				u8g2->drawFrame(x, y, SCREEN_WIDTH - 2 - x, CHARACTER_HEIGHT);
			}
		} 
	    u8g2->sendBuffer();
	}
}

void DisplayContextReal::taskWorkMessage(std::string& taskName, DisplayMessage msg){
	if(taskName == "renderTask"){
		r_lines = msg.lines;
		r_progressDisplay = msg.progressDisplay;
		r_progress = msg.progress;
		r_textFrames = msg.textFrames;
		renderDisplay();
		return;
	}
	throw std::runtime_error("DisplayContextReal::taskWorkMessage invoked with task name " + taskName);
}
