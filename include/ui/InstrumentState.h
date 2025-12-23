#pragma once

#include "UIState.h"

class InstrumentState final : public IUIState {
public:
	UIStateId id() const override { return UIStateId::Instrument; }

	void enter(UIStateContext& context) override;
	void handleAction(const UIAction& action,
						  UIStateContext& context) override;
	void updateDisplay(UIStateContext& context) override;
};
