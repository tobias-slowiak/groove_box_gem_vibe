#pragma once
#include "InternalBelaContext.h"
#include <vector>
#include <math.h>

class BelaContextManager;

class BelaContextRawProcessor
{
public:
	virtual ~BelaContextRawProcessor() {};
	virtual void fromRawToContext(BelaContextManager*, int) = 0;
	virtual void fromContextToRaw(const BelaContextManager*) = 0;
};

class BelaIoMemory{};
/**
 * Interleaved frames in non-interleaved streams:
 * Each of Audio, Analog, Digital streams has a dedicated memory region
 * within which channels are interleaved
 */
class BelaIoMemoryIN : public BelaIoMemory
{
public:
	virtual ~BelaIoMemoryIN() {};
	virtual uint16_t* getAnalogInPtr() = 0;
	virtual uint16_t* getAnalogOutPtr() = 0;
	virtual int16_t* getAudioInPtr() = 0;
	virtual int16_t* getAudioOutPtr() = 0;
};

/**
 * Interleaved frames in interleaved streams:
 * Audio, Analog, Digital streams are interleaved in a single memory region
 * within which channels are interleaved.
 */
class BelaIoMemoryII : public BelaIoMemory
{
};

template <typename T>
class BelaRawAccessor
{
	static_assert(std::is_base_of<BelaIoMemory,T>::value, "BelaRawAccessor template should derive fom BelaIoMemory");
public:
	// these getters will only be implemented via template specialization
	// in order to allow for compiler optimization
	const int16_t getAudioInRaw(unsigned int f, unsigned int c) const;
	const uint16_t getAnalogInRaw(unsigned int f, unsigned int c) const;
	int16_t& getAudioOutRaw(unsigned int f, unsigned int c);
	uint16_t& getAnalogOutRaw(unsigned int f, unsigned int c);
	const uint16_t& getAnalogOutRaw(unsigned int f, unsigned int c) const;
};

template <typename T>
class BelaContextRawProcessorT : public BelaContextRawProcessor
{
	static_assert(std::is_same<T, BelaIoMemoryIN>::value || std::is_same<T,BelaIoMemoryII>::value, "Incorrect type");
public:
	void beforeLoop(const BelaContextManager* context, T* memory);
	void fromRawToContext(BelaContextManager* context, int pruMuxReference) override;
	void fromContextToRaw(const BelaContextManager*) override;
	BelaRawAccessor<T> rawAccessor;
private:
	unsigned int audioInChannels;
	unsigned int analogInChannels;
	unsigned int audioOutChannels;
	unsigned int analogOutChannels;
	unsigned int rawAnalogFrames;
	bool analog_out_is_audio;
	bool interleaved;
	bool analog_enabled;
	bool uniform_sample_rate;
	float analogs_per_audio;
};

class BelaContextManager : public InternalBelaContext
{
public:
	BelaContextManager();
	int setup(BelaHw belaHw, bool uniformSampleRate);
	void cleanup();
	void beforeLoop();
	void beforeRender();
	void afterRender();
	float analogsPerAudio() const { return analogs_per_audio; }
	bool uniformSampleRate() const { return uniform_sample_rate; }
	operator BelaContext*() {
		return context();
	}
	BelaContext* context() {
		return (BelaContext*)(static_cast<InternalBelaContext*>(this));
	}
	BelaContext const* context() const {
		return (BelaContext*)(static_cast<const InternalBelaContext*>(this));
	}
	struct IopSettings
	{
		unsigned int audioFrames {};
		unsigned int audioInChannels {};
		unsigned int audioOutChannels {};
		unsigned int analogFrames {};
		unsigned int analogInChannels {};
		unsigned int analogOutChannels {};
		unsigned int digitalFrames {};
		unsigned int multiplexerChannels {};
		unsigned int adcPerAudioFrame {};
		unsigned int adcIntervalNs {};
		bool remap {};
	};
	const IopSettings& getIopSettings() const { return iopSettings; }
	BelaHw getBelaHw() const { return belaHw; }
private:
	std::vector<float> audioInV;
	std::vector<float> audioOutV;
	std::vector<float> analogInV;
	std::vector<float> analogOutV;
	std::vector<uint32_t> digitalV;
	std::vector<float> multiplexerAnalogInV;
	std::vector<float> last_analog_out_frame;
	std::vector<uint32_t> last_digital_buffer;
	std::vector<float> audio_expander_input_history;
	std::vector<float> audio_expander_output_history;
	float audio_expander_filter_coeff;
	BelaHw belaHw;
	IopSettings iopSettings;
	float analogs_per_audio = 0;
	bool uniform_sample_rate;
};

template<>
class BelaRawAccessor<BelaIoMemoryIN>
{
	unsigned int remapChannel(unsigned int c) const
	{
		if(!hasTrivialMapping())
		{
#ifdef GEMMULTI_REVA3_NOREWORK
			if(1 == c)
				c = 2;
			else if(2 == c)
				c = 1;
#else
			switch(c)
			{
				case 0: return 0;
				case 1: return 3;
				case 2: return 2;
				case 3: return 5;
				case 4: return 7;
				case 5: return 9;
				case 6: return 1;
				case 7: return 4;
				case 8: return 6;
				case 9: return 8;
			}
#endif
		}
		return c;
	}
	unsigned int remapAudioInChannel(unsigned int c) const
	{
		return remapChannel(c);
	}
	unsigned int remapAudioOutChannel(unsigned int c) const
	{
		return remapChannel(c);
	}
public:
	void setup(BelaIoMemoryIN* memory, const BelaContextManager::IopSettings& iopSettings)
	{
		analogInRawPtr = memory->getAnalogInPtr();
		audioInRawPtr = memory->getAudioInPtr();
		analogOutRawPtr = memory->getAnalogOutPtr();
		audioOutRawPtr = memory->getAudioOutPtr();
		s = iopSettings;
	}
	const int16_t getAudioInRaw(unsigned int f, unsigned int c) const
	{
		return audioInRawPtr[f * s.audioInChannels + remapAudioInChannel(c)];
	}
	const uint16_t getAnalogInRaw(unsigned int f, unsigned int c) const
	{
		return analogInRawPtr[f * s.analogInChannels + c];
	}
	int16_t& getAudioOutRaw(unsigned int f, unsigned int c)
	{
		return audioOutRawPtr[f * s.audioOutChannels + remapAudioOutChannel(c)];
	}
	uint16_t& getAnalogOutRaw(unsigned int f, unsigned int c)
	{
		return analogOutRawPtr[f * s.analogOutChannels + c];
	}
	const uint16_t& getAnalogOutRaw(unsigned int f, unsigned int c) const
	{
		return analogOutRawPtr[f * s.analogOutChannels + c];
	}
	bool hasTrivialMapping() const
	{
		return !s.remap;
	}
	const int16_t* audioInRawPtr;
	const uint16_t* analogInRawPtr;
	int16_t* audioOutRawPtr;
	uint16_t* analogOutRawPtr;
private:
	BelaContextManager::IopSettings s;
};
