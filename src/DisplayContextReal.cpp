#include <Bela.h>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <cassert>

#include "../include/IDisplayContext.h"
#include "../include/DisplayContextReal.h"
#include "../include/ResourceManager.h"
#include "../include/DebugLog.h"

///////////////////////////////////////////NONMEMBER-FUNCTIONS

void displayThreadFunction(void* arg) {
	DisplayContextReal* displayContext = static_cast<DisplayContextReal*>(arg);
	assert(displayContext != nullptr);
	displayContext->drainPendingDisplayUpdates();
}




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


DisplayContextReal::DisplayContextReal(ResourceManager* resourceManager, std::vector<U8G2*> u8g2s): resourceManager(resourceManager), u8g2s(u8g2s){
	assert(resourceManager != nullptr);
	lines = std::vector<std::vector<std::string>>{std::vector<std::string>(NUM_LINES,""), std::vector<std::string>(NUM_LINES,"")};
	displayTask = Bela_createAuxiliaryTask(displayThreadFunction, 50, "displayTask", (void*)this);
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
	flushPendingUpdatesSync();
}

void DisplayContextReal::processBlockwise() {
	assert(resourceManager != nullptr);
	if(!hasPendingDisplayUpdate()){
		return;
	}
	bool expected = false;
	if(!displayTaskInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel)){
		return;
	}
	int scheduleResponse = Bela_scheduleAuxiliaryTask(displayTask);
	if(scheduleResponse != 0){
		displayTaskInFlight.store(false, std::memory_order_release);
		if(scheduleResponse != EBUSY){
			DEBUG_RT_PRINTF("DisplayContextReal::processBlockwise(): Bela_scheduleAuxiliaryTask sent error code: %d\n", scheduleResponse);
		}
	}
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
	requestDisplayUpdate();
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines){
	this->lines = lines;
	requestDisplayUpdate();
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
	requestDisplayUpdate();
}

AuxiliaryTask& DisplayContextReal::getDisplayTask() {
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

void DisplayContextReal::requestDisplayUpdate(){
	displayUpdatesRequested.fetch_add(1, std::memory_order_relaxed);
	if(resourceManager != nullptr){
		resourceManager->setUpdateDisplayFlag(true);
	}
}

bool DisplayContextReal::hasPendingDisplayUpdate() const {
	const uint32_t rendered = displayUpdatesRendered.load(std::memory_order_acquire);
	const uint32_t requested = displayUpdatesRequested.load(std::memory_order_acquire);
	return rendered < requested;
}

void DisplayContextReal::drainPendingDisplayUpdates(){
	assert(resourceManager != nullptr);
	while(true){
		const uint32_t target = displayUpdatesRequested.load(std::memory_order_acquire);
		const uint32_t rendered = displayUpdatesRendered.load(std::memory_order_acquire);
		if(rendered >= target){
			break;
		}
		renderDisplay();
		displayUpdatesRendered.store(target, std::memory_order_release);
	}
	if(!hasPendingDisplayUpdate()){
		resourceManager->setUpdateDisplayFlag(false);
	}
	displayTaskInFlight.store(false, std::memory_order_release);
}

void DisplayContextReal::flushPendingUpdatesSync(){
	if(resourceManager == nullptr){
		return;
	}
	while(hasPendingDisplayUpdate()){
		const uint32_t target = displayUpdatesRequested.load(std::memory_order_acquire);
		renderDisplay();
		displayUpdatesRendered.store(target, std::memory_order_release);
	}
	resourceManager->setUpdateDisplayFlag(false);
}
