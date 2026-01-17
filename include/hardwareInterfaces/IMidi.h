#pragma once
//compiel
#include <libraries/Midi/Midi.h>

class IMidiChannelMessage{
public:

	virtual int getDataByte(int index) = 0;
	
	virtual int getChannel() = 0;
	
	virtual MidiMessageType getType() = 0;
	
	virtual void setDataByte(int index, int) = 0;
	
	virtual void setChannel(int) = 0;
	
	virtual void setType(MidiMessageType) = 0;
	
	virtual void prettyPrint() = 0;
};

class IMidiParser{
public:
	virtual int numAvailableMessages() = 0;

	virtual IMidiChannelMessage* getNextChannelMessage() = 0;

	virtual void pushMessage(int note, int velocity, int channel, MidiMessageType type) = 0;
};

class IMidi{
public:
	virtual ~IMidi() = default;
	
	virtual int readFrom(const char*) = 0;
	
	virtual int writeTo(const char*) = 0;
	
	virtual void enableParser(bool) = 0;
	
	virtual IMidiParser* getParser() = 0;

	virtual void writeMessage(midi_byte_t statusCode, midi_byte_t channel, midi_byte_t dataByte1, midi_byte_t dataByte2) = 0;
};