#include <Bela.h>
#include <libraries/Midi/Midi.h>

#include "include/ResourceManager.h"
#include "include/ModeManager.h"
//the following include can be re/decommented to change sth in the render.cpp to force recompilation if it doesnt automatically happen
//#include <math.h>

//this file is only a dispatcher
//in render/... there are render.h files defined, in which setupCase() renderCase() and cleanupCase() are implemented

//TODO: Maybe make naming convention for private members like privateMemberVariable_
//TODO: Reverb effect (where?). ema zamboni sais everything sound good with reverb
//TODO: in deviceMap constructor make the stuff not dependent on ifdef but on a variable that is made at compile time. for that i would also need a way to tell what device is connected.

//TODO: check if it makes sense to do a switch for audio input on/off in case the input creates noise.

/*
TODO: the following calls cause a drop of blocks on the first call, warm them up in the setup:
std::to_string(float)
something in the voices class, check what -> TODO: why does the first initialized voice always cause a drop of blocks?
*/

ResourceManager* resourceManager;


bool setup(BelaContext *context, void *userData)
{
	resourceManager = new ResourceManager();
	resourceManager->setup(context);
	printf("setup done\n");
	return true;
}

void render(BelaContext *context, void *userData)
{
	resourceManager->getModeManager()->render(context, resourceManager);
}

void cleanup(BelaContext *context, void *userData)
{
	resourceManager->cleanup(context);
}