#include <Bela.h>
#include <libraries/Midi/Midi.h>
#include <cassert>

#include "include/ResourceManager.h"
#include "include/ModeManager.h"


//TODO: make stack allocation instead of heap pointers wherever possible.
//   where it is not possible (for example the real/fake dual classes) do in place initiation of unique ptrs.
/*
like this:
class ResourceManager {
public:
    ResourceManager(bool interfaceConnected,
                    const DeviceMap& dev)
    : interface(interfaceConnected
        ? std::make_unique<BelaInterface>(*this)
        : std::unique_ptr<BelaInterface>{}), // empty when not connected
      keyMidi([&]{
        if(auto m = std::make_unique<MidiReal>();
           m->readFrom(dev.keyMidiName.c_str()) == 1) {
            m->writeTo(dev.keyMidiName.c_str());
            m->enableParser(true);
            return m;
        }
        return std::make_unique<MidiFake>();
      }()),
      display(interfaceConnected
        ? std::make_unique<DisplayContextReal>(*this, initU8G2s())
        : std::make_unique<DisplayContextFake>())
    {}

private:
    std::unique_ptr<BelaInterface> interface;
    std::unique_ptr<IMidi> keyMidi;
    std::unique_ptr<IDisplayContext> display;
};
*/

//TODO: change all .at() for assert+operator[]

//TODO: wherever possible put checkAndWorkMessages out of processBlockwise and only run when state changed.

//TODO: put mount of SD card in the run on boot.

//TODO: dieses deamon und restart in startup (warum muss das immer gemacht werden?)

//the following include can be re/decommented to change sth in the render.cpp to force recompilation if it doesnt automatically happen
//#include <math.h>

//this file is only a dispatcher
//in render/... there are render.h files defined, in which setupCase() renderCase() and cleanupCase() are implemented

//TODO: looper test is still in the blockwise processing mode, change it to framewise processing mode
//TODO: work out streaming form sd card
//TODO make one big buffer also for samplers not only for loopers then also the SamplerS class makes sense.
//TODO: Maybe make naming convention for private members like privateMemberVariable_
//TODO: Reverb effect (where?). ema zamboni sais everything sound good with reverb
//TODO: in deviceMap constructor make the stuff not dependent on ifdef but on a variable that is made at compile time. for that i would also need a way to tell what device is connected.

//TODO: check if it makes sense to do a switch for audio input on/off in case the input creates noise.

/*
TODO: the following calls cause a drop of blocks on the first call, warm them up in the setup:
std::to_string(float)
something in the voices class, check what -> TODO: why does the first initialized voice always cause a drop of blocks?
*/

/*
TODO: make ResourceManager::updateDisplay to setUpdateDisplayFlag(bool) to be 
the same as the steaming buffer flag.
Also. make the thread flag thing unified in general. only use the atomic bools
via getters and setters in the resource manager (or is another way better?)*/

/*
Notes:
- Do not create 2 auxiliary tasks using the same obj. bela will reacti with malloc() or corrupted size vs. prev. size errors!
- The prio of auxiliary tasks is not allowed to be 100
- if i put rt_printf instead of printf in the auxiliary task i get very strange runtime errors. DONT!
- debug prints on auxiliary tasks make the debugging more confusing because timing is irgendwos
*/

ResourceManager* resourceManager;


bool setup(BelaContext *context, void *userData)
{
	assert(context != nullptr);
	resourceManager = new ResourceManager();
	resourceManager->setup(context);
	printf("setup done\n");
	return true;
}

void render(BelaContext *context, void *userData)
{
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	resourceManager->getModeManager()->render(context, resourceManager);
}

void cleanup(BelaContext *context, void *userData)
{
	assert(context != nullptr);
	assert(resourceManager != nullptr);
	resourceManager->cleanup(context);
}
