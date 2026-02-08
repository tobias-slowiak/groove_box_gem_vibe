#pragma once
#include <stdint.h>
#include <string>
#include <string.h>
#include "Mcasp.h"

struct AudioCodecParams {
	typedef enum {
		kTdmModeI2s,
		kTdmModeDsp,
		kTdmModeTdm,
	} TdmMode;
	typedef enum {
		kClockSourceMcasp,
		kClockSourceCodec,
		kClockSourceExternal,
	} ClockSource;
	unsigned int numDataLines; // number of data lines (in each direction if codec is bidirectional)
	unsigned int numSlots; // number of TDM slots
	unsigned int slotSize; // size of a slot in bits
	unsigned int startingSlot; // what slot in the TDM frame to place the first channel in
	unsigned int bitDelay; // additional offset in the TDM frame (in bits)
	double mclk; // frequency of the master clock passed to the codec
	double samplingRate; // audio sampling rate
	TdmMode tdmMode; // what TDM mode to use
	ClockSource bclk; // who generates the bit clock
	ClockSource wclk; // who generates the frame sync
	void print();
	AudioCodecParams()
	{
		// initialise the padding to 0 so we can later memcmp ...
		// ridiculous. Thanks C++
		memset(this, 0, sizeof(*this));
	}
	bool operator== (const AudioCodecParams& other) const
	{
		return !memcmp(this, &other, sizeof(*this));
	}
	bool operator!= (const AudioCodecParams& other) const
	{
		return !(other == *this);
	}
};

class AudioCodec
{
public:
	virtual ~AudioCodec() {};
	virtual int initCodec() = 0;
	virtual int startAudio(int shouldBeReady) = 0;
	virtual int stopAudio() = 0;
	virtual unsigned int getNumIns() = 0;
	virtual unsigned int getNumOuts() = 0;
	virtual float getSampleRate() = 0;
	virtual int setInputGain(int channel, float newGain) = 0;
	virtual int setLineOutVolume(int channel, float gain) = 0;
	virtual int setHpVolume(int channel, float gain) = 0;
	virtual int disable() = 0;
	virtual int reset() = 0;
	virtual int setMode(std::string parameter) {return 0;};
	virtual McaspConfig& getMcaspConfig() = 0;
};

class AudioCodecDummy : public AudioCodec
{
public:
	AudioCodecDummy(float sampleRate, size_t numIns, size_t numOuts) :
		sampleRate(sampleRate),
		numIns(numIns),
		numOuts(numOuts)
		{}
	int initCodec() { return 0; }
	int startAudio(int shouldBeReady) { return 0; }
	int stopAudio() { return 0; }
	unsigned int getNumIns() { return numIns; }
	unsigned int getNumOuts() { return numOuts; }
	float getSampleRate() { return sampleRate; }
	int setInputGain(int channel, float newGain) { return 0; }
	int setLineOutVolume(int channel, float gain) { return 0; }
	int setHpVolume(int channel, float gain) { return 0; }
	int disable() { return 0; }
	int reset() { return 0; }
	int setMode(std::string parameter) {return 0; }
	McaspConfig& getMcaspConfig() { return mcaspConfig; }
private:
	float sampleRate;
	size_t numIns;
	size_t numOuts;
	McaspConfig mcaspConfig;
};
