#pragma once

#include <libraries/Midi/Midi.h>
#include "IMidi.h"

class MidiChannelMessageReal: public IMidiChannelMessage{
public:
	MidiChannelMessageReal(){
		belaMsg = new MidiChannelMessage();
	}
	
	MidiChannelMessage* getBelaMessage(){return belaMsg;}
	
	int getDataByte(int index) override {return belaMsg->getDataByte(index);}
	
	int getChannel() override {return belaMsg->getChannel();}
	
	MidiMessageType getType() override {return belaMsg->getType();}
	
	void setDataByte(int index, int val) override {belaMsg->setDataByte(index, val);}
	
	void setChannel(int ch) override {belaMsg->setChannel(ch);}
	
	void setType(MidiMessageType type) override {belaMsg->setType(type);}
	
	void prettyPrint() override {belaMsg->prettyPrint();}
private:
	MidiChannelMessage* belaMsg;
};

class MidiParserReal: public IMidiParser{
public:
	MidiParserReal(MidiParser* parser): belaParser(parser){
		msg = new MidiChannelMessageReal();
	}
	
	int numAvailableMessages() override {return belaParser->numAvailableMessages();}

	IMidiChannelMessage* getNextChannelMessage() override {
		MidiChannelMessage nextBelaMsg = belaParser->getNextChannelMessage();
		msg->setDataByte(0, nextBelaMsg.getDataByte(0));
		msg->setDataByte(1, nextBelaMsg.getDataByte(1));
		msg->setType(nextBelaMsg.getType());
		msg->setChannel(nextBelaMsg.getChannel());
		return msg;
	}
private:
	MidiParser* belaParser;
	IMidiChannelMessage* msg;
};

class MidiReal: public IMidi{
public:
	MidiReal(){
		midi = new Midi();
		parser = new MidiParserReal(midi->getParser());
	}
	int readFrom(const char* port) override {return midi->readFrom(port);}
	
	int writeTo(const char* port) override {return midi->writeTo(port);}
	
	void enableParser(bool arg) override {midi->enableParser(arg);}
	
	IMidiParser* getParser() override {return parser;}
private:
	Midi* midi;
	IMidiParser* parser;
};