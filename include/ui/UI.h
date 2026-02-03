#pragma once
//compiel
#include <memory>
#include <vector>
class ResourceManager;
class InterfaceMessage;
class IMidiChannelMessage;
#include "../audio/Mixer.h"

class UI;
class Metronome;
class Loopers;

struct UIParamManipulator {
	std::function<void()> increase;
	std::function<void()> decrease;
	std::function<void()> select;
};

struct UIParamLine {
    std::string label; // fixed label text
    std::function<std::string()> value;
	UIParamManipulator manipulator;
};

enum class UIStateId {
	General,
	Instrument,
	Sampler,
	COUNT
};

struct UIState {
	friend class UI;
	UIState(UI& ui, UIStateId id);
	UIStateId id;
	std::vector<UIParamLine> paramLines;
	int currentParamLineIndex = 0;
	bool paramLineInEditMode = false;
	std::vector<std::vector<UIParamLine>> subParamLines;
	int currentSubParamSetIndex = 0;
	int currentSubParamLineIndex = 0;
	bool subParamLineInEditMode = false;
};


struct UIStateContext {
	UIStateContext(ResourceManager& rm);
	ResourceManager& rm;
	Metronome& metronome;
	int blocksPerDisplayUpdate = 275; //approx 10 Hz updaterate
	int blocksElapsedSinceLastDisplayUpdate = 0;
	Loopers& loopers;
	ResourceManager& getRM() {return rm;}
	UIStateId stateId = UIStateId::General;
	int instrumentIndex = 0;
	int drumIndex = 0;
	int samplerIndex = 0;
	int sliceIndex = 0;
	int numberOfSliceForAutoSlice = 2;
	size_t numberOfBarsForLoopers = 1;
	bool triggerOffInDrumMode = false; //TODO: maybe make a dedicated drum class that handles this?
};

enum class stateNavigationEvent {
	Up,
	Down,
	Push
};

class UI {
public:
	UI(ResourceManager& rm);

	void menuUp(int displayId);
	void menuDown(int displayId);
	void menuPush(int displayId);

	void setGain(GainId gainId, float value);
	void setLooperGain(int looperId, float value);

	void triggerVoice(int note, int velocity, bool melodicMode);
	void triggerOff(int note, bool melodicMode);

	void looperTogglePlay(int looperId);
	void looperToggleRecord(int looperId);

	void masterTogglePlay();
	void masterToggleRecord();

	void processBlockwise();
	void updateDisplay();
	void renderDisplay();

	void stateSwitch(int indexShift);
	void drumKeySwitch();
	void metronomeStateSwitch();
	void micToggle();

private:
	friend struct UIState;
	bool updateDisplayFlag = false;
	UIStateContext ctxt;
	std::vector<UIState> states;
	UIState& state;
};