#pragma once

#include <string>
#include <vector>
#include "UIAction.h"
#include "../general/ResourceManager.h"
#include "../hardwareInterfaces/IDisplayContext.h"
#include "../audio/InstrumentCatalog.h"
#include "../audio/Mixer.h"

//attention! when inserting or removing a state the ctor of the statemachine needs to be manually updated too.
enum class UIStateId {
	General,
	Instrument,
	Sampler,
	COUNT
};


struct UIStateContext {
	UIStateContext(ResourceManager& rm)
		: displayContext(rm.getDisplayContext()),
		instrumentCatalog(rm.getInstrumentCatalog()),
		mixer(rm.getMixer()){}
	IDisplayContext& displayContext;
	InstrumentCatalog& instrumentCatalog;
	Mixer& mixer;
	int totalNumberOfStates = 3;
	UIStateId stateId = UIStateId::General;
	int bpm = 120;
	int instrumentIndex = 0;
	int samplerIndex = 0;
	int sliceIndex = 0;
	int numberOfSliceForAutoSlice = 2;
};

class IUIState {
public:
	virtual ~IUIState() = default;
	virtual UIStateId id() const = 0;

	virtual void enter(UIStateContext& context) {
		(void)context;
	}

	virtual void handleAction(const UIAction& action,
						  UIStateContext& context) = 0;

	virtual void updateDisplay(UIStateContext& context) = 0;
};
