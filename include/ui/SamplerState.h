#pragma once

#include "UIState.h"

class SamplerState final : public IUIState {
public:
	UIStateId id() const override { return UIStateId::Sampler; }

	void enter(UIStateContext& context) override;
	void handleAction(const UIAction& action,
						  UIStateContext& context) override;
	void updateDisplay(UIStateContext& context) override;
};
