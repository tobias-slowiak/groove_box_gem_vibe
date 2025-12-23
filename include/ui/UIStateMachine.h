#pragma once

#include <memory>
#include <vector>
#include "UIState.h"
#include "../hardwareInterfaces/IDisplayContext.h"
#include "../general/ResourceManager.h"


class UIStateMachine {
public:
	UIStateMachine(ResourceManager& rm);

	void handleAction(const UIAction& action);
	
	void stateSwitch(int indexShift);

private:
	std::vector<IUIState> states;
	UIStateContext context;
};
