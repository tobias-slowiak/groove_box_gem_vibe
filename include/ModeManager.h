#pragma once 
#include <Bela.h>
#include <vector>
#include <string>

class ResourceManager;

enum class Mode{
	TopMenu,
	Normal,
	AllTest,
	BelaInterfaceTest,
	DisplayContextTest,
	VoicesTest,
	LoopersTest,
	SamplersTest,
	MidiTest,
	ControllerTest,
	SamplePack,
	StreamingBandwidthTest,
	COUNT
};

class ModeManager{
public:
	ModeManager(ResourceManager* resourceManager);
	
	void render(BelaContext *context, ResourceManager* resourceManager);

	void renderTopMenu(BelaContext* context, ResourceManager* resourceManager);
	
	void renderNormal(BelaContext *context, ResourceManager* resourceManager);
	
	void renderBelaInterfaceTest(BelaContext *context, ResourceManager* resourceManager);
	
	void renderDisplayContextTest(BelaContext *context, ResourceManager* resourceManager);
	
	void renderVoicesTest(BelaContext *context, ResourceManager* resourceManager);
	
	void renderLoopersTest(BelaContext *context, ResourceManager* resourceManager);
	
	void renderSamplersTest(BelaContext *context, ResourceManager* resourceManager);
	
	void renderMidiTest(BelaContext *context, ResourceManager* resourceManager);
	
	void renderControllerTest(BelaContext *context, ResourceManager* resourceManager);

	void renderSamplePackTest(BelaContext *context, ResourceManager* resourceManager);

	void renderStreamingBandwidthTest(BelaContext *context, ResourceManager* resourceManager);
	
	void modeShift(int indexShift, Mode* mode);
	
	bool secondsElapsed(int blocksElapsed, float seconds);

	float blocksToSeconds(int blocksElapsed);
private:
	ResourceManager* resourceManager;
	BelaContext* context;
	Mode mode;
	bool currentTestDone = false;
	bool testAll = false;
	std::vector<std::string> modeNames;
};