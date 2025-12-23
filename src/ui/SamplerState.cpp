#include "../../include/ui/SamplerState.h"

void SamplerState::enter(UIStateContext& context) {
	updateDisplay(context);
}

void SamplerState::handleAction(const UIAction& action,
						  UIStateContext& context) {
	(void)action;
	(void)context;
}

void updateDisplay(UIStateContext& c){
	c.displayContext.setLines(
		{
			{
			"smplr: " + std::to_string(c.samplerIndex),
			"NrSamplers: " + std::to_string(2), //TODO: implement getNrSamplers in Samplers class
			"push for new",
			""
			},
			
			{
			"Slice: " + std::to_string(c.sliceIndex),
			"NrSlices: " + std::to_string(3), //TODO: make this real
			"push for new" ,
			"0: auto: " + std::to_string(c.numberOfSliceForAutoSlice)
			}
		}
	);
}