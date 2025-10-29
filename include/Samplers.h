#pragma once

#include <Bela.h>
#include <vector>

class ResourceManager;


class Sampler{
public:
    Sampler(float timeInSeconds);

	void startRecord();
	
	bool isRecording();
    
    void recordFrame(float in);
    
    void newSlice();
    
    int getNrSlices();
    
    std::pair<float*, int> getSampleSlice(int sliceIndex);
    
    void setStart(int sliceIndex, float percentage);
    
    void setEnd(int sliceIndex, float percentage);
    
	float getFrame(int index);
    
    float getPlaybackRate(int note);
    
private:
	int numberOfFrames = 0;
    //TODO: make one big buffer for all samplers
	std::vector<float> samplerBuffer;
    int bufferReadIndex = 0;
    int numberOfSlices = 0;
	bool recording = false;
    std::vector<float> playbackRates;
    std::vector<int> starts;
    std::vector<int> ends;
};


class Samplers {
public:
	Samplers(ResourceManager* resourceManager): resourceManager(resourceManager) {}
    
	void newSampler(float timeInSeconds);

	void startRecord(int samplerIndex);

	bool isRecording(int samplerIndex);
    
    void process(float inFrame);
    
    void newSlice(int samplerIndex);

	int getNrSlices(int samplerIndex);
    
    std::pair<float*, int> getSampleSlice(int samplerIndex, int sliceIndex);
    
    void setStart(int samplerIndex, int sliceIndex, float percentage);
    
    void setEnd(int samplerIndex, int sliceIndex, float percentage);
    
	float getFrame(int samplerIndex, int index);
    
    float getPlaybackRate(int samplerIndex, int note);

private:
	ResourceManager* resourceManager;
	std::vector<Sampler> samplers;
};
