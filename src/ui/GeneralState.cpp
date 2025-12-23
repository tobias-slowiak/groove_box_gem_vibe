#include "../../include/ui/GeneralState.h"
#include "../../include/audio/Mixer.h"
#include "../../include/hardwareInterfaces/IDisplayContext.h"
#include "../../include/audio/InstrumentCatalog.h"

void GeneralState::enter(UIStateContext& context) {
	updateDisplay(context);
}

void GeneralState::handleAction(const UIAction& action,
						  UIStateContext& context) {
	(void)action;
	(void)context;
}

void updateDisplay(UIStateContext& c){
	c.displayContext.setLines(
		{
			{
			"bpm: " + std::to_string(c.bpm),
			"inst: " + c.instrumentCatalog.getDisplayName(c.instrumentIndex),
			"",
			""
			},
			
			{
			"MasterGain: " + std::to_string(c.mixer.masterGain),
			"InputGain: " + std::to_string(c.mixer.inputGain),
			"",
			""
			}
		}
	);
}