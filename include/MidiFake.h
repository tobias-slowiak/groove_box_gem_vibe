#pragma once

#include <Bela.h>
#include <libraries/Midi/Midi.h>
#include "IMidi.h"

class MidiChannelMessageFake: public IMidiChannelMessage{
public:
	MidiChannelMessageFake() {}
	
	int getDataByte(int index) override {return -1;}
	
	int getChannel() override {return -1;}
	
	MidiMessageType getType() override {return kmmNoteOff;}
	
	void setDataByte(int index, int val) override {}
	
	void setChannel(int ch) override {}
	
	void setType(MidiMessageType type) override {}
	
	void prettyPrint() override {rt_printf("prettyPrint() MidiChannelMessageFake\n");}
};

class MidiParserFake: public IMidiParser{
public:
	MidiParserFake() {msg = new MidiChannelMessageFake();}
	
	int numAvailableMessages() override {return 0;}

	IMidiChannelMessage* getNextChannelMessage() override {return msg;}
private:
	IMidiChannelMessage* msg;
};

class MidiFake: public IMidi{
public:
	MidiFake() {parser = new MidiParserFake();}
	
	int readFrom(const char* port) override {return 1;}
	
	int writeTo(const char* port) override {return 1;}
	
	void enableParser(bool arg) override {}
	
	IMidiParser* getParser() override {return parser;}
private:
	IMidiParser* parser;
};