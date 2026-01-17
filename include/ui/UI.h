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

struct UIParamLine {
    std::string label; // fixed label text
    std::function<std::string()> value;
};

struct UIParamManipulator {
	std::function<void()> increase;
	std::function<void()> decrease;
	std::function<void()> select;
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
	std::vector<UIParamManipulator> manipulators;
	std::vector<std::vector<UIParamManipulator>> subParamManipulators;
	bool subParamLineInEditMode = false;
};


struct UIStateContext {
	UIStateContext(ResourceManager& rm);
	ResourceManager& rm;
	Metronome& metronome;
	Loopers& loopers;
	ResourceManager& getRM() {return rm;}
	UIStateId stateId = UIStateId::General;
	int instrumentIndex = 0;
	int drumIndex = 0;
	int samplerIndex = 0;
	int sliceIndex = 0;
	int numberOfSliceForAutoSlice = 2;
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

	void updateDisplay();
	void updateLooperLights();

	void stateSwitch(int indexShift);
	void drumKeySwitch();
	void metronomeStateSwitch();
	void micToggle();

private:
	friend struct UIState;
	UIStateContext ctxt;
	std::vector<UIState> states;
	UIState& state;
};