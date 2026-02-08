#pragma once
#include "Bela.h"
#include <string>

namespace BelaHwComponent {
typedef enum {
	BelaCape,
	BelaCapeRevC,
	BelaMiniCape,
	GemStereo,
	GemMulti,
	GemBase,
	CtagCape,
	PocketBeagle,
	BeagleBoneBlack,
	PocketBeagle2,
	Tlv320aic3104,
} Component;
}
BelaHw Bela_detectUserHw();
bool Bela_checkHwCompatibility(BelaHw userHw, BelaHw detectedHw);
BelaHw getBelaHw(std::string board);
std::string getBelaHwName(BelaHw hardware);
unsigned int Bela_hwContains(BelaHw hw, BelaHwComponent::Component component);
