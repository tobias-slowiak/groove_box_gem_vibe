#pragma once
#include <stdint.h>
#include <vector>
static const uint32_t MCASP_PIN_AFSX = 1 << 28;
static const uint32_t MCASP_PIN_AHCLKX = 1 << 27;
static const uint32_t MCASP_PIN_ACLKX = 1 << 26;
static const uint32_t MCASP_PIN_AMUTE = 1 << 25; // Also, 0 to 3 are XFR0 to XFR3

class McaspConfig
{
public:
	struct Registers
	{
		uint32_t pdir;
		uint32_t rmask;
		uint32_t rfmt;
		uint32_t afsrctl;
		uint32_t aclkrctl;
		uint32_t ahclkrctl;
		uint32_t rtdm;
		uint32_t xmask;
		uint32_t xfmt;
		uint32_t afsxctl;
		uint32_t aclkxctl;
		uint32_t ahclkxctl;
		uint32_t xtdm;
		uint8_t srctln[16] = {};
		uint32_t wfifoctl;
		uint32_t rfifoctl;
	};
	struct Runtime
	{
		// the below is raw data we pass to the PRU, so it needs to
		// match the layout in the pru file
		uint16_t outChannels {};
		uint16_t inChannels {};
		uint32_t outSerializersDisabledSubSlots {};
		uint32_t inSerializersDisabledSubSlots {};
	};
	struct Parameters
	{
		struct SerMask {
			unsigned int axr {};
			uint32_t enableMask {};
		};
		std::vector<SerMask> inSerializers {};
		std::vector<SerMask> outSerializers {};
		unsigned int numSlots {};
		unsigned int slotSize {};
		unsigned int dataSize {};
		unsigned int bitDelay {};
		double auxClkIn {};
		double ahclkFreq {};
		bool ahclkIsInternal {};
		bool aclkIsInternal {};
		bool wclkIsInternal {};
		bool wclkIsWord {};
		bool wclkFalling {};
		bool externalSamplesRisingEdge {};
	};
	typedef enum {
		SrctlMode_DISABLED = 0,
		SrctlMode_TX = 1,
		SrctlMode_RX = 2,
	} SrctlMode;
	typedef enum {
		SrctlDrive_TRISTATE = 0,
		SrctlDrive_LOW = 2,
		SrctlDrive_HIGH = 3,
	} SrctlDrive;
	McaspConfig();
	void print();
	Parameters params;
	Registers getRegisters();
	Runtime getRuntime();
private:
	Runtime r;
	static uint32_t computeTdm(unsigned int numSlots);
	static uint32_t computeFifoctl(unsigned int numSerializers);
	int setFmt();
	int setAfsctl();
	int setAclkctl();
	int setAhclkctl();
	int setPdir();
	int setSrctln(unsigned int n, McaspConfig::SrctlMode mode, McaspConfig::SrctlDrive drive);
	int setChannels(bool input);
	int checkSerializers();
public:
	Registers regs;
};

namespace Mcasp {
	void start(McaspConfig config);
	double getValidAhclk(double desiredClock, unsigned int* outDiv = nullptr);
	void stop();
};
