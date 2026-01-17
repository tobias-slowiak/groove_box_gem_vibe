#include "../../include/general/ResourceManager.h"
#include "../../include/ui/UI.h"
#include "../../include/hardwareInterfaces/BelaInterface.h"
//compiel


//TODO: the manual ctor here is unelegant. think of a better version.
UI::UI(ResourceManager& resourceManager)
	: ctxt(resourceManager),
	states([this]() {
						std::vector<UIState> v;
						for(int i = 0; i < (int)UIStateId::COUNT; i++){
							v.push_back(UIState(*this, (UIStateId)i));
						}
						return v;
					}()),
	state(VEC_AT(states, 0)) {
	//TODO: add other states here

	//enter initial state
	updateDisplay();
}

void UI::menuUp(int displayId){
	if(displayId == 0){
		if(state.paramLineInEditMode){
			state.manipulators[state.currentParamLineIndex].increase();
		}else{
			state.currentParamLineIndex--;
			if(state.currentParamLineIndex < 0) state.currentParamLineIndex = state.paramLines.size() - 1;
			if(state.currentParamLineIndex < state.subParamLines.size()) state.currentSubParamSetIndex = state.currentParamLineIndex;
		}
	}else{
		if(state.subParamLineInEditMode){
			state.subParamManipulators[state.currentSubParamSetIndex][state.currentSubParamLineIndex].increase();
		}else{
			state.currentSubParamLineIndex--;
			if(state.currentSubParamLineIndex < 0) state.currentSubParamLineIndex = state.subParamLines[state.currentSubParamSetIndex].size() - 1;
		}
	}
	updateDisplay();
}

void UI::menuDown(int displayId){
	if(displayId == 0){
		if(state.paramLineInEditMode){
			state.manipulators[state.currentParamLineIndex].decrease();
		}else{
			state.currentParamLineIndex++;
			if(state.currentParamLineIndex >= (int)state.paramLines.size()) state.currentParamLineIndex = 0;
			if(state.currentParamLineIndex < state.subParamLines.size()) state.currentSubParamSetIndex = state.currentParamLineIndex;
		}
	}else{
		if(state.subParamLineInEditMode){
			state.subParamManipulators[state.currentSubParamSetIndex][state.currentSubParamLineIndex].decrease();
		}else{
			state.currentSubParamLineIndex++;
			if(state.currentSubParamLineIndex >= (int)state.subParamLines[state.currentSubParamSetIndex].size()) state.currentSubParamLineIndex = 0;
		}
	}
	updateDisplay();
}

void UI::menuPush(int displayId){
	if(displayId == 0){
		if(VEC_AT(state.paramLines, state.currentParamLineIndex).value() == ""){
			//these parameters only open the subparameter set - nothing to edit here
			return;
		}
		state.manipulators[state.currentParamLineIndex].select();
		state.paramLineInEditMode = !state.paramLineInEditMode;
	}else{
		state.subParamManipulators[state.currentSubParamSetIndex][state.currentSubParamLineIndex].select();
		state.subParamLineInEditMode = !state.subParamLineInEditMode;
	}
	updateDisplay();
}

void UI::setGain(GainId gainId, float value){
	ctxt.rm.getMixer().setGain(gainId, value);
}

void UI::setLooperGain(int looperId, float value){
	//TODO
}

void UI::triggerVoice(int note, int velocity, bool melodicMode){
		if(melodicMode){
			ctxt.rm.getKeyInstrumentSamplePack().triggerVoice(note, velocity);
		}else{
			ctxt.rm.getDrumSamplePack().triggerVoice(note, velocity);
		}
}

void UI::triggerOff(int note, bool melodicMode){
	if(melodicMode){
		ctxt.rm.getKeyInstrumentSamplePack().triggerOff(note);
	}else{
		ctxt.rm.getDrumSamplePack().triggerOff(note);
	}
}

void UI::looperTogglePlay(int looperId){
	ctxt.rm.getLoopers().togglePlay(looperId);
}

void UI::looperToggleRecord(int looperId){
	ctxt.rm.getLoopers().toggleRecord(looperId);
}

void UI::masterTogglePlay(){
	//TODO
}
void UI::masterToggleRecord(){
	//TODO
}

void UI::updateDisplay(){
	std::vector<std::vector<std::string>> liness(2);
	auto& mainParamLines = state.paramLines;
	auto& subParamLines = VEC_AT(state.subParamLines, state.currentSubParamSetIndex);
	int frameLine = ctxt.rm.NUM_LINES_PER_DISPLAY / 2 - (ctxt.rm.NUM_LINES_PER_DISPLAY % 2 == 0 ? 1 : 0);
	std::vector<int> frameStartChars = {0,0};
	int currentParamIndex, nrParamLines;
	for(int displayId = 0; displayId < 2; displayId++){
		std::vector<UIParamLine>& paramLines = (displayId == 0) ? mainParamLines : subParamLines;
		if(displayId == 0){
			if(state.paramLineInEditMode){
				VEC_AT(frameStartChars, displayId) = VEC_AT(paramLines, state.currentParamLineIndex).label.size() + 2;
			}
			currentParamIndex = state.currentParamLineIndex;
			nrParamLines = paramLines.size();
		} else {
			if(state.subParamLineInEditMode){
				VEC_AT(frameStartChars, displayId) = VEC_AT(paramLines, state.currentSubParamLineIndex).label.size() + 2;
			}
			currentParamIndex = state.currentSubParamLineIndex;
			nrParamLines = paramLines.size();
		}
		int nrEmptyLines = ctxt.rm.NUM_LINES_PER_DISPLAY - nrParamLines;
		int maxshift = nrParamLines / 2 - (nrParamLines % 2 == 0 ? 1 : 0);
		std::vector<std::string>& lines = VEC_AT(liness, displayId);
		for(int shift = -frameLine; shift <= ctxt.rm.NUM_LINES_PER_DISPLAY - frameLine - 1; shift++){
			int index = (currentParamIndex + shift + nrParamLines) % nrParamLines;
			if(nrEmptyLines > 0){
				if(nrParamLines % 2 == 0 && shift == -maxshift - 1){
					lines.push_back(VEC_AT(paramLines, index).label + ": " + VEC_AT(paramLines, index).value());
					continue;
				}
				if(std::abs(shift) > maxshift){
					lines.push_back("");
					continue;
				}
			}
			lines.push_back(VEC_AT(paramLines, index).label + ": " + VEC_AT(paramLines, index).value());
		}
	}
	
	std::vector<std::vector<TextFrame>> textFrames(2);
	for(int displayId = 0; displayId < 2; displayId++){
		VEC_AT(textFrames, displayId).push_back(TextFrame{VEC_AT(frameStartChars, displayId), frameLine});
	}
	ctxt.rm.getDisplayContext().setLines(liness, textFrames);
}

void UI::updateLooperLights(){
	//TODO
}



void UI::stateSwitch(int indexShift){
	if(indexShift != 1 && indexShift != -1) throw std::runtime_error("stateSwitch() used with indexshift unequal 1 or -1");
	int stateIndex = ((int)ctxt.stateId + indexShift) % (int)UIStateId::COUNT;
	if(stateIndex < 0) stateIndex += (int)UIStateId::COUNT;
	if(stateIndex == (int)UIStateId::COUNT) stateIndex += indexShift;
	state = VEC_AT(states, stateIndex);
	ctxt.stateId = (UIStateId)(stateIndex);

	updateDisplay();
}

void UI::drumKeySwitch(){
	//TODO
}

void UI::micToggle(){
	//TODO
}
