#include "../../include/ui/InstrumentState.h"

void InstrumentState::enter(UIStateContext& context) {
	updateDisplay(context);
}

void InstrumentState::handleAction(const UIAction& action,
						  UIStateContext& context) {
	(void)action;
	(void)context;
}

void updateDisplay(UIStateContext& c){
	c.displayContext.setLines(
		{
			{
			"InstGain: " + std::to_string(c.mixer.instrumentGain),
			"inst: " + c.instrumentCatalog.getDisplayName(c.instrumentIndex),
			"",
			""
			},
			
			{
			"",
			"",
			"",
			""
			}
		}
	);
}