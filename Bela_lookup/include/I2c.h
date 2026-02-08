#pragma once

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
// heuristic to guess what version of i2c-dev.h we have:
// the one installed with `apt-get install libi2c-dev`
// would conflict with linux/i2c.h, while the stock
// one requires linus/i2c.h
#ifndef I2C_SMBUS_BLOCK_MAX
// If this is not defined, we have the "stock" i2c-dev.h
// so we include linux/i2c.h
#include <linux/i2c.h>
typedef unsigned char i2c_char_t;
#else
typedef char i2c_char_t;
#endif
#include <sys/ioctl.h>
#ifdef GEMMULTI_REVA_NOREWORK
#include <Mmap.h>
#include <mutex>
#endif

#define MAX_BUF_NAME 64

class I2c
{
protected:
	int i2C_bus;
	int i2C_address;
	int i2C_file;
	ssize_t doIoctl(int address, i2c_char_t* data, int length, unsigned int flags);
	void enableDevice();
public:
	ssize_t readBytes(void* buf, size_t count);
	ssize_t writeBytes(const void* buf, size_t count);
	I2c(){};
	I2c(I2c&&) = delete;
	int initI2C_RW(int bus, int address, int file);
	int closeI2C();
	ssize_t readRaw(int address, i2c_char_t *data, int length, bool repeated = false);
	ssize_t writeRaw(int address, const i2c_char_t *data, int length, bool repeated = false);
	virtual ~I2c();
};

inline void I2c::enableDevice()
{
	// quirks in case you have a multiplexer or something like that
#ifdef GEMMULTI_REVA_NOREWORK
	static volatile uint32_t* pinmux;
	static std::once_flag flag;
	std::call_once(flag, [](){
		static Mmap map;
		pinmux = (uint32_t*)map.map(0xf4000, 4096);
	});

	// we change the pinmuxing on 0x1e8 and 0x1ec:
	if(1 == i2C_bus || 4 == i2C_bus)
	{
		uint32_t val;
		if(1 == i2C_bus)
			val = 0x50000; // mode 0, I2C peripheral
		else
			val = 0x50007; // mode 7, GPIO, gpio-i2c driver, for bus 4
		uint8_t offsets[] = {
			0x1e8 / sizeof(*pinmux),
			0x1ec / sizeof(*pinmux),
		};
		for(size_t n = 0; n < sizeof(offsets); ++n)
			pinmux[offsets[n]] = val;
	}
#endif // GEMMULTI_REVA_NOREWORK
}

inline int I2c::initI2C_RW(int bus, int address, int fileHnd)
{
	i2C_bus 	= bus;
	i2C_address = address;
	i2C_file 	= fileHnd;

	// open I2C device as a file
	char namebuf[MAX_BUF_NAME];
	snprintf(namebuf, sizeof(namebuf), "/dev/i2c-%d", i2C_bus);

	if ((i2C_file = open(namebuf, O_RDWR)) < 0)
	{
		fprintf(stderr, "Failed to open %s I2C Bus\n", namebuf);
		return(1);
	}

	// target device as slave
	enableDevice();
	if (ioctl(i2C_file, I2C_SLAVE, i2C_address) < 0){
		fprintf(stderr, "I2C_SLAVE address %#x failed...", i2C_address);
		return(2);
	}

	return 0;
}



inline int I2c::closeI2C()
{
	if(i2C_file > 0) {
		if(close(i2C_file) > 0)
			return 1;
		else
			i2C_file = -1;
	}
	return 0;
}

inline ssize_t I2c::readBytes(void *buf, size_t count)
{
	enableDevice();
	return ::read(i2C_file, buf, count);
}

inline ssize_t I2c::writeBytes(const void *buf, size_t count)
{
	enableDevice();
	return ::write(i2C_file, buf, count);
}

inline ssize_t I2c::doIoctl(int address, i2c_char_t* data, int length, unsigned int flags)
{
	enableDevice();
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg message;
	message.addr = address;
	message.flags = flags;
	message.len = length;
	message.buf = data;
	packets.msgs = &message;
	packets.nmsgs = 1;
	int ret;
	if((ret = ioctl(i2C_file, I2C_RDWR, &packets)) < 0)
	{
		fprintf(stderr, "Failed to do ioctl for %d bytes on I2C at addr %d with flags %d: %d\n", length, address, flags, ret);
		return -1;
	}
	return  length;

}
inline ssize_t I2c::readRaw(int address, i2c_char_t *data, int length, bool repeated)
{
	unsigned int flags = I2C_M_RD | (repeated ? I2C_M_NOSTART : 0);
	return doIoctl(address, data, length, flags);
}

inline ssize_t I2c::writeRaw(int address, const i2c_char_t *data, int length, bool repeated)
{
	unsigned int flags = (repeated ? I2C_M_NOSTART : 0);
	return doIoctl(address, (i2c_char_t*)data, length, flags);
}

inline I2c::~I2c(){
	if(i2C_file >= 0)
		close(i2C_file);
}
