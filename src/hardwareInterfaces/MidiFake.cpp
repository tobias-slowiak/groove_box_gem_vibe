#include <Bela.h>
#include <libraries/Midi/Midi.h>
#include <vector>
#include <cassert>
#include "../../include/hardwareInterfaces/IMidi.h"
#include "../../include/hardwareInterfaces/MidiFake.h"
//compiel

IMidiChannelMessage* MidiParserFake::getNextChannelMessage() {
    if(popIndex >= DEFAULT_QUEUE_SIZE) popIndex = 0;
    assert(popIndex <= pushIndex && "popindex overtook push, should not happen");
    MidiChannelMessageFake* res = &(messages[popIndex]);
    popIndex++;
    return res;
}

void MidiParserFake::pushMessage(int note, int velocity, int channel, MidiMessageType type) {
    if(pushIndex >= DEFAULT_QUEUE_SIZE) pushIndex = 0;
    MidiChannelMessageFake* pushMessage = &(messages[pushIndex]);
    assert(pushMessage != nullptr);
    pushMessage->setDataByte(0, note);
    pushMessage->setDataByte(1, velocity);
    pushMessage->setChannel(channel);
    pushMessage->setType(type);
    pushIndex++;
}
