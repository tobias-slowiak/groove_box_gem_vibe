#pragma once

#include <stdint.h>
#include "Bela.h"
#include "Gpio.h"
#include "PruManager.h"
#include "BelaContextManager.h"
#include <Mcasp.h>

class PruMemory;
class BelaContextRawProcessorPru;
class PRU
{
public:
	// Constructor
	PRU(BelaContextManager *input_context);
	// Destructor
	~PRU();
	// Initialise and open the PRU
	int initialise(BelaHw newBelaHw, int pru_num,
				   Gpio::Pin stopButtonPin, bool enableLed,
				   uint32_t disabledBelaDigitalChannels);

	// Run the code image in pru_rtaudio_bin.h
	int start(const char * const filename, const McaspConfig::Runtime& mcaspRuntime);

	// Loop: read and write data from the PRU and call the user-defined audio callback
	void loop(void *userData, void(*render)(BelaContext*, void*), bool highPerformanceMode, BelaCpuData* cpuData);
private:
	// Prepare the GPIO pins needed for the PRU
	int prepareGPIO(int include_led);
	// Clean up the GPIO at the end
	void cleanupGPIO();
	void initialisePruCommon(const McaspConfig::Runtime& mcaspRuntime, unsigned int pru_audio_out_channels, unsigned int hardware_analog_frames);
	int testPruError();
	PruManager* pruManager = nullptr;
	BelaContextManager* const context;

	int pru_number;		// Which PRU we use
	bool running;		// Whether the PRU is running
	bool analog_enabled;  // Whether SPI ADC and DAC are used
	bool digital_enabled; // Whether digital is used
	bool led_enabled;	// Whether a user LED is enabled

	PruMemory* pruMemory = nullptr;
	BelaContextRawProcessorPru* rawProcessor = nullptr;
	volatile uint32_t *pru_buffer_comm;
	uint32_t pruBufferMcaspFrames;
	bool pruUsesMcaspIrq;
	BelaHw belaHw;

	Gpio stopButton; // Monitoring the bela cape button
	Gpio underrunLed; // Flashing an LED upon underrun
	Gpio adcNrstPin; // Resetting the ADC on Bela Mini Rev C
	Gpio adcChipSelect; // Chip select for ADC
	Gpio dacChipSelect; // Chip select for DAC
	Gpio onboardLed;
	std::vector<Gpio> belaDigitals;
	uint32_t disabledDigitalChannels;
};
