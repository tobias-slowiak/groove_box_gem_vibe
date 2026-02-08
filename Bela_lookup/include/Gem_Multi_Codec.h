/*
 * Gem_Multi_Codec.h
 *
 * Wrapper class for a four-codec combination:
 * TLV320AIC3106 as main clock source and stereo I/O
 * ES9080 for 8 outputs
 * 2x TLV320ADC3140 for 4 inputs each
 *
 */

#pragma once

#include <vector>
#include <memory>
#include "I2c_Codec.h"
#include "Es9080_Codec.h"
#include "Tlv320Adc3140_Codec.h"

class Gem_Multi_Codec : public AudioCodec
{
public:
	int initCodec();
	int startAudio(int shouldBeReady);
	int stopAudio();
	unsigned int getNumIns();
	unsigned int getNumOuts();
	float getSampleRate();

	int setInputGain(int channel, float newGain);
	int setLineOutVolume(int channel, float gain);
	int setHpVolume(int channel, float gain);
	int disable();
	int reset();
	int setMode(std::string parameter);

	McaspConfig& getMcaspConfig();

	Gem_Multi_Codec(int tlvI2cBus, int tlvI2cAddr, I2c_Codec::CodecType tlvType, 
					int esI2cBus, int esI2cAddr, Gpio::Pin esResetPin, 
					int tlvAdcI2cBus, int tlvAdcI2cBusAddr1, int tlvAdcI2cBusAddr2,
					double sampleRate, bool isVerbose);
	~Gem_Multi_Codec();

protected:
	McaspConfig mcaspConfig;

private:
	I2c_Codec* tlv320aic310x;
	Es9080_Codec* es9080;
	Tlv320Adc3140_Codec* adc3140_1;
	Tlv320Adc3140_Codec* adc3140_2;

	bool running;
	bool verbose;
};
