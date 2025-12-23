#include "../../include/general/ResourceManager.h"

void ResourceManager::processBlockwise(){

}

float ResourceManager::process(int n){
    float instrumentFrame = voices->process();
    float inputFrameL = audioRead(context, n, 0);
    float inputFrameR = audioRead(context, n, 1);

    return mixer.mix(instrumentFrame, inputFrameL, inputFrameR);
}