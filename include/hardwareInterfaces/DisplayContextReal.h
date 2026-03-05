#pragma once

#include <Bela.h>
#include "u8g2/U8g2LinuxI2C.h"
#include <atomic>
#include <vector>
#include <stdexcept>
#include <cstdint>
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

	void setWaveformOverlay(bool enabled,
						   const std::vector<float>& waveform,
						   float sliceStartNormalized,
						   float sliceEndNormalized,
						   bool editStartBoundary) override;
    
    ResourceManager& getResourceManager() { return resourceManager; }
		
	void renderDisplay() override;

    void taskWorkMessage(std::string& taskName, DisplayMessage msg);
	
private:
    void maybeRunWatchdog(uint64_t nowNs);
    void maybeQueueDeferredDisplayUpdate();
    void recoverDisplayI2C();

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
    bool waveformOverlayEnabled = false;
    std::vector<float> waveform;
    float waveformSliceStartNormalized = 0.0f;
    float waveformSliceEndNormalized = 1.0f;
    bool waveformEditStartBoundary = true;

    //render Task only
    std::vector<std::vector<std::string>> r_lines;
    int r_progressDisplay = -1; 
    float r_progress = 0.0f;
	std::vector<std::vector<TextFrame>> r_textFrames;
	std::vector<ScrollBar> r_scrollBars;
	bool r_waveformOverlayEnabled = false;
	std::vector<float> r_waveform;
	float r_waveformSliceStartNormalized = 0.0f;
	float r_waveformSliceEndNormalized = 1.0f;
	bool r_waveformEditStartBoundary = true;

    static constexpr int MARQUEE_GAP_CHARS = 3;
    static constexpr uint64_t MARQUEE_STEP_NS = 110000000ULL;
    struct MarqueeState {
        std::string key;
        size_t offset = 0;
        uint64_t lastStepNs = 0;
        bool active = false;
    };
    std::vector<MarqueeState> r_marqueeStates;
    bool r_hasMarqueeAnimation = false;
    std::atomic<bool> marqueeAnimationNeeded{false};
    std::atomic<bool> marqueeTickPending{false};
    uint64_t marqueeLastTickEnqueueNs = 0;
    std::atomic<bool> displayUpdatePending{false};
    std::atomic<bool> deferredDisplayUpdate{false};
    std::atomic<bool> watchdogRecoveryPending{false};
    std::atomic<uint64_t> taskHeartbeatCounter{0};
    std::atomic<uint64_t> taskHeartbeatNs{0};
    uint64_t watchdogLastSeenHeartbeat = 0;
    uint64_t watchdogLastSeenHeartbeatNs = 0;
    uint64_t watchdogLastRecoveryNs = 0;
    static constexpr uint64_t WATCHDOG_STALL_NS = 1500000000ULL;
    static constexpr uint64_t WATCHDOG_RECOVERY_COOLDOWN_NS = 1000000000ULL;

    
};
