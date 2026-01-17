#include "../../include/hardwareInterfaces/IMidi.h"
#include "../../include/hardwareInterfaces/MidiReal.h"

void MidiChannelMessageReal::prettyPrint(){
    if(belaMsg->getChannel() == 8){
        return; //launchkey 38 mk3 spam too much
    }
    belaMsg->prettyPrint();
}