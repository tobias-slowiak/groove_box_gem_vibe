/*
 * Tlv320Adc3140_Codec.h
 *
 * Supports the TLV320ADC3140 four-channel ADC with TDM audio data and I2C configuration.
 */

#pragma once

#include "AudioCodec.h"
#include "I2c.h"
#include <array>
#include <Gpio.h>

class Tlv320Adc3140_Codec : public I2c, public AudioCodec
{
public:
	typedef enum {
		AnalogDifferential = 0,
		AnalogSingleEnded,
		DigitalPdm,
	} InputSource;
	typedef enum {
		Impedance2k5 = 0,
		Impedance10k,
		Impedance20k,
	} InputImpedance;
	typedef enum {
		FilterTypeLinear = 0,
		FilterTypeLowLatency,
		FilterTypeUltraLowLatency,
	} FilterType;

	Tlv320Adc3140_Codec(int i2cBus, int i2cAddress, AudioCodecParams::ClockSource clockSource, Gpio::Pin resetPin, double mclkFrequency, bool isVerbose);
	~Tlv320Adc3140_Codec();
	
	int writeRegister(unsigned int reg, unsigned int value);
	int readRegister(unsigned int reg);

	int initCodec();
	int setParameters(const AudioCodecParams& codecParams);
	AudioCodecParams getParameters();
	int startAudio(int shouldBeReady);
	int stopAudio();
	unsigned int getNumIns();
	unsigned int getNumOuts();
	float getSampleRate();

	int setInputGain(int channel, float gain);
	int setLineOutVolume(int channel, float gain) { return 0; }
	int setHpVolume(int channel, float gain) { return 0; }
	int disable();
	int reset();
	
	int setInputSource(int channel, InputSource source, bool writeToCodec = false);
	int setInputDcCoupling(int channel, bool dc, bool writeToCodec = false);
	int setInputImpedance(int channel, InputImpedance impedance, bool writeToCodec = false);
	int setFilterType(FilterType filter, bool writeToCodec = false);
	int setDigitalHighpassFilter(bool enableFilter, bool writeToCodec = false);
	int setMicBias(double voltage, bool writeToCodec = false);

	void setStartingSlot(int slot);

	void setVerbose(bool isVerbose);

	virtual McaspConfig& getMcaspConfig();
private:
	enum {kNumIoChannels = 4};
	int writeChannelConfigRegisters(int channel);
	int writeDspConfigRegisters();
	int writeMicBiasRegisters();
	int writeInputLevelRegisters(int channel, bool mute);
protected:
	int configureDCRemovalIIR(bool enable); //called by startAudio()
	std::array<float,kNumIoChannels> inputGain{};
	std::array<InputSource,kNumIoChannels> inputSource{};
	std::array<bool,kNumIoChannels> inputDc{};
	std::array<InputImpedance,kNumIoChannels> inputImpedance{};
	AudioCodecParams params;
	McaspConfig mcaspConfig;
	bool running;
	bool verbose;
	bool gotMcaspConfig = false;
	double micBias;
	bool enableHighpassFilter;
	FilterType filterType;
};
