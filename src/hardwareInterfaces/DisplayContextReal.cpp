#include <Bela.h>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <cmath>
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
	this->scrollBars.clear();
	sendTaskMessage();
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines, std::vector<std::vector<TextFrame>> textFrames){
	this->lines = lines;
	this->textFrames = textFrames;
	this->scrollBars.clear();
	sendTaskMessage();
}

void DisplayContextReal::setLines(std::vector<std::vector<std::string>> lines,
								  std::vector<std::vector<TextFrame>> textFrames,
								  std::vector<ScrollBar> scrollBars){
	this->lines = lines;
	this->textFrames = textFrames;
	this->scrollBars = scrollBars;
	sendTaskMessage();
}

void DisplayContextReal::sendTaskMessage(){
	DisplayMessage msg;
	msg.lines = lines;
	msg.progressDisplay = progressDisplay;
	msg.progress = progress;
	msg.textFrames = textFrames;
	msg.scrollBars = scrollBars;
    msg.waveformOverlayEnabled = waveformOverlayEnabled;
    msg.waveform = waveform;
    msg.waveformSliceStartNormalized = waveformSliceStartNormalized;
    msg.waveformSliceEndNormalized = waveformSliceEndNormalized;
    msg.waveformEditStartBoundary = waveformEditStartBoundary;
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

void DisplayContextReal::setWaveformOverlay(bool enabled,
                                            const std::vector<float>& waveform,
                                            float sliceStartNormalized,
                                            float sliceEndNormalized,
                                            bool editStartBoundary){
    waveformOverlayEnabled = enabled;
    this->waveform = waveform;
    waveformSliceStartNormalized = sliceStartNormalized;
    waveformSliceEndNormalized = sliceEndNormalized;
    waveformEditStartBoundary = editStartBoundary;
}


void DisplayContextReal::renderDisplay() {
	for(int display = 0; display < 2; display++){
	    assert(u8g2s.size() > static_cast<size_t>(display));
	    U8G2* u8g2 = u8g2s.at(display);
	    if(r_textFrames.empty()){
	    	u8g2->clearDisplay();
	    }
	    u8g2->clearBuffer();
        if(display == 0 && r_waveformOverlayEnabled){
            const int midY = SCREEN_HEIGHT / 2;
            const int maxAmp = std::max(1, SCREEN_HEIGHT / 2 - 2);
            if(!r_waveform.empty()){
                for(int x = 0; x < SCREEN_WIDTH; ++x){
                    size_t waveformIndex = 0;
                    if(SCREEN_WIDTH > 1 && r_waveform.size() > 1){
                        waveformIndex = (static_cast<size_t>(x) * (r_waveform.size() - 1)) / static_cast<size_t>(SCREEN_WIDTH - 1);
                    }
                    float value = VEC_AT(r_waveform, waveformIndex);
                    value = std::max(-1.0f, std::min(1.0f, value));
                    int amp = static_cast<int>(value * static_cast<float>(maxAmp));
                    int y0 = midY;
                    int y1 = midY - amp;
                    int yMin = std::min(y0, y1);
                    int height = std::max(1, std::abs(y1 - y0) + 1);
                    u8g2->drawVLine(x, yMin, height);
                }
            }

            int startX = static_cast<int>(r_waveformSliceStartNormalized * static_cast<float>(SCREEN_WIDTH - 1));
            int endX = static_cast<int>(r_waveformSliceEndNormalized * static_cast<float>(SCREEN_WIDTH - 1));
            startX = std::max(0, std::min(startX, SCREEN_WIDTH - 1));
            endX = std::max(0, std::min(endX, SCREEN_WIDTH - 1));
            if(endX < startX){
                std::swap(startX, endX);
            }

            u8g2->drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            u8g2->drawVLine(startX, 0, SCREEN_HEIGHT);
            u8g2->drawVLine(endX, 0, SCREEN_HEIGHT);

            int markerX = r_waveformEditStartBoundary ? startX : endX;
            markerX = std::max(1, std::min(markerX, SCREEN_WIDTH - 2));
            u8g2->drawBox(markerX - 1, 0, 3, 5);
            u8g2->sendBuffer();
            continue;
        }

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
		const bool hasScrollBar = r_scrollBars.size() > static_cast<size_t>(display) &&
			r_scrollBars.at(display).enabled;
		if(r_textFrames.size() > display){
			std::vector<TextFrame>& textFramesOnThisDisplay = VEC_AT(r_textFrames, display);
			const int rightPadding = hasScrollBar ? 8 : 2;
			for(auto frame: textFramesOnThisDisplay){
				int x = frame.startChar * CHARACTER_WIDTH; //approx char width
				int y = frame.textLine * CHARACTER_HEIGHT - 1;
				int frameWidth = std::max(1, SCREEN_WIDTH - rightPadding - x);
				u8g2->drawHLine(x, y, frameWidth);
				u8g2->drawHLine(x, y + CHARACTER_HEIGHT - 1, frameWidth);
				u8g2->drawVLine(x + frameWidth - 1, y, CHARACTER_HEIGHT);
			}
		}
		if(hasScrollBar){
			const ScrollBar& scrollBar = r_scrollBars.at(display);
			if(scrollBar.totalItems > 0 && scrollBar.visibleItems > 0){
				const int trackX = SCREEN_WIDTH - 4;
				const int trackY = 0;
				const int trackH = SCREEN_HEIGHT;
				const int visibleItems = std::min(scrollBar.visibleItems, scrollBar.totalItems);
				const int proportionalThumb = (trackH * visibleItems) / scrollBar.totalItems;
				const int thumbH = std::max(4, std::min(trackH / 3, proportionalThumb));
				const int maxThumbOffset = std::max(0, trackH - thumbH);
				const int maxSelected = std::max(0, scrollBar.totalItems - 1);
				const int clampedSelected = std::max(0, std::min(scrollBar.selectedIndex, maxSelected));
				const int thumbOffset = (maxSelected == 0) ? 0 : (clampedSelected * maxThumbOffset) / maxSelected;
				u8g2->drawVLine(trackX, trackY, trackH);
				u8g2->drawBox(trackX - 1, trackY + thumbOffset, 3, thumbH);
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
		r_scrollBars = msg.scrollBars;
        r_waveformOverlayEnabled = msg.waveformOverlayEnabled;
        r_waveform = msg.waveform;
        r_waveformSliceStartNormalized = msg.waveformSliceStartNormalized;
        r_waveformSliceEndNormalized = msg.waveformSliceEndNormalized;
        r_waveformEditStartBoundary = msg.waveformEditStartBoundary;
		renderDisplay();
		return;
	}
	throw std::runtime_error("DisplayContextReal::taskWorkMessage invoked with task name " + taskName);
}
