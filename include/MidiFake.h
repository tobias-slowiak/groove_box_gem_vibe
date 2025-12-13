#pragma once

#include <Bela.h>
#include <libraries/Midi/Midi.h>
#include <vector>
#include "IMidi.h"

class MidiChannelMessageFake: public IMidiChannelMessage{
public:
	MidiChannelMessageFake() {}
	
	int getDataByte(int index) override {return dataBytes.at(index);}
	
	int getChannel() override {return channel;}
	
	MidiMessageType getType() override {return type;}
	
	void setDataByte(int index, int val) override {dataBytes.at(index) = val;}
	
	void setChannel(int ch) override {channel = ch;}
	
	void setType(MidiMessageType t) override {type = t;}
	
	void prettyPrint() override {rt_printf("MidiChannelMessageFake: data:%d_%d ch:%d, type(conv):%d\n",dataBytes.at(0), dataBytes.at(1), channel, type);}
private:
	std::vector<int> dataBytes = std::vector<int>(2,0);
	int channel = 0;
	MidiMessageType type = kmmNoteOff;
};

class MidiParserFake: public IMidiParser{
public:
	MidiParserFake() {}
	
	int numAvailableMessages() override {return pushIndex - popIndex;}

	IMidiChannelMessage* getNextChannelMessage() override;

	void pushMessage(int note, int velocity, int channel, MidiMessageType type) override;
private:
	static constexpr int DEFAULT_QUEUE_SIZE = 100;
	MidiChannelMessageFake messages[DEFAULT_QUEUE_SIZE]{};
	int pushIndex = 0;
	int popIndex = 0;
};

class MidiFake: public IMidi{
public:
	MidiFake() {}
	
	int readFrom(const char* port) override {return 1;}
	
	int writeTo(const char* port) override {return 1;}
	
	void enableParser(bool arg) override {}
	
	IMidiParser* getParser() override {return &parser;}
private:
	MidiParserFake parser;
};