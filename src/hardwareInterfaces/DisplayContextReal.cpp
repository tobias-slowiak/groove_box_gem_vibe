#include <Bela.h>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <chrono>
//compil
#include "../../include/hardwareInterfaces/IDisplayContext.h"
#include "../../include/hardwareInterfaces/DisplayContextReal.h"
#include "../../include/general/ResourceManager.h"
#include "../../include/general/DebugLog.h"
#include "../../include/general/TaskWrapper.h"

namespace {
uint64_t steadyNowNs(){
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}
}



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
			  renderTask(this, 70, "renderTask"),
              r_marqueeStates(std::vector<MarqueeState>(2)){
	r_lines = lines;
	if(numLines < 3 || numLines > 5){
		throw std::runtime_error("DisplayContextReal: invalid display parameters");
	}
    const uint64_t nowNs = steadyNowNs();
    taskHeartbeatNs.store(nowNs, std::memory_order_release);
    watchdogLastSeenHeartbeatNs = nowNs;
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
    maybeQueueDeferredDisplayUpdate();
    const uint64_t nowNs = steadyNowNs();
    maybeRunWatchdog(nowNs);
    if(!marqueeAnimationNeeded.load(std::memory_order_acquire)){
        return;
    }

    if(nowNs - marqueeLastTickEnqueueNs < MARQUEE_STEP_NS){
        return;
    }
    if(nowNs - lastDisplayPushEnqueueNs < DISPLAY_PUSH_MIN_INTERVAL_NS){
        return;
    }

    bool expectedPending = false;
    if(!marqueeTickPending.compare_exchange_strong(expectedPending, true, std::memory_order_acq_rel)){
        return;
    }

    DisplayMessage tickMsg;
    tickMsg.tickOnly = true;
    try{
        renderTask.pushMessage(TaskMessageTarget::TaskThread, tickMsg);
        marqueeLastTickEnqueueNs = nowNs;
        lastDisplayPushEnqueueNs = nowNs;
        renderTask.taskCheckAndWorkMessages();
    } catch(const std::runtime_error&){
        // Never crash due to marquee animation pressure; skip this frame and retry later.
        marqueeTickPending.store(false, std::memory_order_release);
    }
}

void DisplayContextReal::maybeQueueDeferredDisplayUpdate(){
    if(!deferredDisplayUpdate.load(std::memory_order_acquire)){
        return;
    }
    if(displayUpdatePending.load(std::memory_order_acquire)){
        return;
    }
    const uint64_t nowNs = steadyNowNs();
    if(nowNs - lastDisplayPushEnqueueNs < DISPLAY_PUSH_MIN_INTERVAL_NS){
        return;
    }
    if(!deferredDisplayUpdate.exchange(false, std::memory_order_acq_rel)){
        return;
    }
    sendTaskMessage();
    renderTask.taskCheckAndWorkMessages();
}

void DisplayContextReal::maybeRunWatchdog(uint64_t nowNs){
    const uint64_t heartbeat = taskHeartbeatCounter.load(std::memory_order_acquire);
    if(heartbeat != watchdogLastSeenHeartbeat){
        watchdogLastSeenHeartbeat = heartbeat;
        watchdogLastSeenHeartbeatNs = taskHeartbeatNs.load(std::memory_order_acquire);
        return;
    }

    const bool waitingForTask =
        displayUpdatePending.load(std::memory_order_acquire) ||
        marqueeTickPending.load(std::memory_order_acquire) ||
        watchdogRecoveryPending.load(std::memory_order_acquire);
    if(!waitingForTask){
        return;
    }

    if(nowNs - watchdogLastSeenHeartbeatNs < WATCHDOG_STALL_NS){
        return;
    }

    // First attempt: re-kick the scheduler if the task is not currently in flight.
    if(!renderTask.isInFlight()){
        renderTask.setScheduleFlag();
        renderTask.taskCheckAndWorkMessages();
    }

    // If the task is still in flight, avoid forcing anything unsafe.
    if(renderTask.isInFlight()){
        return;
    }

    if(watchdogRecoveryPending.load(std::memory_order_acquire)){
        return;
    }
    if(nowNs - watchdogLastRecoveryNs < WATCHDOG_RECOVERY_COOLDOWN_NS){
        return;
    }

    DisplayMessage recoverMsg;
    recoverMsg.tickOnly = true;
    recoverMsg.recoverDisplay = true;
    try{
        renderTask.pushMessage(TaskMessageTarget::TaskThread, recoverMsg);
        watchdogRecoveryPending.store(true, std::memory_order_release);
        watchdogLastRecoveryNs = nowNs;
        renderTask.taskCheckAndWorkMessages();
        rt_printf("Display watchdog: queued display recovery\n");
    } catch(const std::runtime_error&){
        // Queue pressure: keep running and retry on next watchdog interval.
    }
}

void DisplayContextReal::recoverDisplayI2C(){
    for(auto* u8g2: u8g2s){
        if(!u8g2){
            continue;
        }
        u8g2->initDisplay();
        u8g2->setPowerSave(0);
        u8g2->setFont(FONT);
        u8g2->setFontRefHeightText();
        u8g2->setFontPosTop();
        u8g2->clearBuffer();
        u8g2->sendBuffer();
    }
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
    const uint64_t nowNs = steadyNowNs();
    if(nowNs - lastDisplayPushEnqueueNs < DISPLAY_PUSH_MIN_INTERVAL_NS){
        deferredDisplayUpdate.store(true, std::memory_order_release);
        return;
    }

    bool expectedPending = false;
    if(!displayUpdatePending.compare_exchange_strong(expectedPending, true, std::memory_order_acq_rel)){
        deferredDisplayUpdate.store(true, std::memory_order_release);
        return;
    }

	DisplayMessage msg;
	msg.lines = lines;
	msg.progressDisplay = progressDisplay;
	msg.progress = progress;
	msg.textFrames = textFrames;
	msg.scrollBars = scrollBars;
    msg.tickOnly = false;
    msg.waveformOverlayEnabled = waveformOverlayEnabled;
	msg.waveform = waveform;
	msg.waveformSliceStartNormalized = waveformSliceStartNormalized;
	msg.waveformSliceEndNormalized = waveformSliceEndNormalized;
	msg.waveformEditStartBoundary = waveformEditStartBoundary;
    try{
	    renderTask.pushMessage(TaskMessageTarget::TaskThread, msg);
        lastDisplayPushEnqueueNs = nowNs;
    } catch(const std::runtime_error&){
        displayUpdatePending.store(false, std::memory_order_release);
        deferredDisplayUpdate.store(true, std::memory_order_release);
        throw;
    }
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
    const uint64_t nowNs = steadyNowNs();
    r_hasMarqueeAnimation = false;
	for(int display = 0; display < 2; display++){
	    assert(u8g2s.size() > static_cast<size_t>(display));
	    U8G2* u8g2 = u8g2s.at(display);
	    if(r_textFrames.empty()){
	    	u8g2->clearDisplay();
	    }
	    u8g2->clearBuffer();
	        if(display == 0 && r_waveformOverlayEnabled){
                if(r_marqueeStates.size() > static_cast<size_t>(display)){
                    r_marqueeStates.at(display).active = false;
                    r_marqueeStates.at(display).key.clear();
                    r_marqueeStates.at(display).offset = 0;
                    r_marqueeStates.at(display).lastStepNs = nowNs;
                }
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
	    std::vector<std::string> displayLines = r_lines.at(display);
		const bool hasScrollBar = r_scrollBars.size() > static_cast<size_t>(display) &&
			r_scrollBars.at(display).enabled;
        bool displayHasActiveMarquee = false;
        if(r_textFrames.size() > static_cast<size_t>(display)){
            const std::vector<TextFrame>& textFramesOnThisDisplay = r_textFrames.at(display);
            const int rightPadding = hasScrollBar ? 8 : 2;
            for(const TextFrame& frame : textFramesOnThisDisplay){
                if(frame.startChar <= 0){
                    continue;
                }
                if(frame.textLine < 0 || static_cast<size_t>(frame.textLine) >= displayLines.size()){
                    continue;
                }
                const int x = frame.startChar * CHARACTER_WIDTH;
                const int frameWidthPx = std::max(1, SCREEN_WIDTH - rightPadding - x);
                const int frameChars = std::max(1, frameWidthPx / CHARACTER_WIDTH);

                std::string& lineText = VEC_AT(displayLines, frame.textLine);
                const int prefixLen = std::max(0, std::min(frame.startChar, static_cast<int>(lineText.size())));
                const std::string prefix = lineText.substr(0, static_cast<size_t>(prefixLen));
                const std::string value = lineText.substr(static_cast<size_t>(prefixLen));
                if(static_cast<int>(value.size()) <= frameChars){
                    continue;
                }

                if(r_marqueeStates.size() <= static_cast<size_t>(display)){
                    r_marqueeStates.resize(static_cast<size_t>(display) + 1);
                }
                MarqueeState& marquee = r_marqueeStates.at(display);
                const std::string marqueeKey =
                    std::to_string(frame.textLine) + "|" +
                    std::to_string(frame.startChar) + "|" +
                    std::to_string(frameChars) + "|" + value;
                if(!marquee.active || marquee.key != marqueeKey){
                    marquee.active = true;
                    marquee.key = marqueeKey;
                    marquee.offset = 0;
                    marquee.lastStepNs = (nowNs > MARQUEE_STEP_NS) ? (nowNs - MARQUEE_STEP_NS) : 0;
                }

                const size_t cycleLength = value.size() + static_cast<size_t>(MARQUEE_GAP_CHARS);
                while(nowNs >= marquee.lastStepNs + MARQUEE_STEP_NS){
                    marquee.lastStepNs += MARQUEE_STEP_NS;
                    marquee.offset = (marquee.offset + 1) % cycleLength;
                }

                std::string looped = value + std::string(MARQUEE_GAP_CHARS, ' ') + value;
                while(looped.size() < marquee.offset + static_cast<size_t>(frameChars)){
                    looped += std::string(MARQUEE_GAP_CHARS, ' ');
                    looped += value;
                }

                lineText = prefix + looped.substr(marquee.offset, static_cast<size_t>(frameChars));
                displayHasActiveMarquee = true;
                r_hasMarqueeAnimation = true;
            }
        }
        if(!displayHasActiveMarquee && r_marqueeStates.size() > static_cast<size_t>(display)){
            r_marqueeStates.at(display).active = false;
            r_marqueeStates.at(display).key.clear();
            r_marqueeStates.at(display).offset = 0;
            r_marqueeStates.at(display).lastStepNs = nowNs;
        }

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
    marqueeAnimationNeeded.store(r_hasMarqueeAnimation, std::memory_order_release);
	}

void DisplayContextReal::taskWorkMessage(std::string& taskName, DisplayMessage msg){
	if(taskName == "renderTask"){
        if(msg.recoverDisplay){
            recoverDisplayI2C();
        }
        if(!msg.tickOnly){
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
        }
		renderDisplay();
        taskHeartbeatNs.store(steadyNowNs(), std::memory_order_release);
        taskHeartbeatCounter.fetch_add(1, std::memory_order_acq_rel);
        if(!msg.tickOnly){
            displayUpdatePending.store(false, std::memory_order_release);
        }
        if(msg.tickOnly){
            marqueeTickPending.store(false, std::memory_order_release);
        }
        if(msg.recoverDisplay){
            watchdogRecoveryPending.store(false, std::memory_order_release);
        }
		return;
	}
	throw std::runtime_error("DisplayContextReal::taskWorkMessage invoked with task name " + taskName);
}
