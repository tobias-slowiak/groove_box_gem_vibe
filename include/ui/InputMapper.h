#pragma once

#include <vector>
#include "InputEvent.h"
#include "UIAction.h"

class InputMapper {
public:
	std::vector<UIAction> map(const std::vector<InputEvent>& events) const;
};
