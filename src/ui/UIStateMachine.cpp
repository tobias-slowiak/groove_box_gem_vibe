#include "../../include/ui/UIStateMachine.h"
#include "../../include/ui/GeneralState.h"
#include "../../include/ui/InstrumentState.h"
#include "../../include/ui/SamplerState.h"
#include "../../include/ui/GeneralState.h"
#include "../../include/ui/InstrumentState.h"
#include "../../include/ui/SamplerState.h"

//TODO: the manual ctor here is unelegant. think of a better version.
UIStateMachine::UIStateMachine(ResourceManager& rm)
	: states(
	{
		GeneralState(),
		InstrumentState(),
		SamplerState()
	}
	), context(rm) {}

void UIStateMachine::handleAction(const UIAction& action) {
	if(action.type == UIActionType::StateNext) {
		stateSwitch(+1);
		VEC_AT(states, (int)context.stateId).enter(context);
		return;
	}
	if(action.type == UIActionType::StatePrev) {
		stateSwitch(-1);
		VEC_AT(states, (int)context.stateId).enter(context);
		return;
	}
	VEC_AT(states, (int)context.stateId).handleAction(action, context);
}

void UIStateMachine::stateSwitch(int indexShift){
	if(indexShift != 1 && indexShift != -1) throw std::runtime_error("stateSwitch() used with indexshift unequal 1 or -1");
	int stateIndex = ((int)context.stateId + indexShift) % (int)UIStateId::COUNT;
	if(stateIndex < 0) stateIndex += (int)UIStateId::COUNT;
	if(stateIndex == (int)UIStateId::COUNT) stateIndex += indexShift;
	context.stateId = (UIStateId)(stateIndex);
}