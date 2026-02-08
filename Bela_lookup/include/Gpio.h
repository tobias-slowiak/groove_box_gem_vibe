#pragma once
#include <cstdint>
#include "Mmap.h"

class Gpio{
public:
	struct Pin {
		uint16_t mod = -1;
		uint16_t num = -1;
		uint32_t toInt() const {
			return (mod << 16 | num);
		}
		static Pin fromInt(uint32_t in) {
			struct Pin gpio;
			gpio.mod = in >> 16;
			gpio.num = in & 0xffff;
			return gpio;
		}
		bool operator ==(const Pin& gpio) const {
			return gpio.mod == mod && gpio.num == num;
		}
		bool operator !=(const Pin& gpio) const {
			return !(gpio == *this);
		}
	};
	static constexpr Pin kInvalidPin {0xffff, 0xffff};
	Gpio();

	~Gpio();

	typedef enum {
		INPUT,
		OUTPUT,
	} Direction;

	/**
	 * Opens a GPIO pin.
	 *
	 * @param pin the GPIO pin ( 0 <= pin < 128)
	 * @param direction one of `INPUT` or `OUTPUT`
	 * @param unexport if `false`, it will not try to unexport the pin when calling `close()`
	 *
	 * @return 0 if success, -1 otherwise;
	 */
	int open(Pin gpioPin, Direction direction, bool unexport = true);
	
	/**
	 * Closes a currently open GPIO
	 */
	void close();

	/**
	 * Read the GPIO value.
	 * @return the GPIO value
	 */
	bool read(){
		return (gpioAddr[GPIO_DATAIN] & pinMask);
	}

	/**
	 * Set the output to 1.
	 */
	void set(){
		gpioAddr[GPIO_SETDATAOUT] = pinMask;
	}

	/** Clear the output
	 */
	void clear(){
		gpioAddr[GPIO_CLEARDATAOUT] = pinMask;
	}

	/**
	 * Write an output value
	 * @param value the value to write
	 */
	void write(bool value){
		if(value){
			set();
		} else {
			clear();
		}
	}

	/**
	 * Check if the GPIO is enabled.
	 *
	 * @return true if enabled, false otherwise
	 */
	bool enabled(){
		return nullptr != gpioAddr;
	}

	Pin getPin() {
		return gpio;
	}
	/**
	 * A utility function to return the mask from a Gpio number.
	 */
	static uint32_t getMask(Pin pin);
	/**
	 * A utility function to return the bank from a Gpio number.
	 */
	static uint32_t getBankNumber(Pin gpioPin);
	/**
	 * A utility function to return the base address of a Gpio bank.
	 */
	static uint32_t getBankAddress(unsigned int bank);
private:
#ifdef IS_AM62
	// offsets with respect to GPIOx_DIRyz
	static constexpr uint32_t GPIO_DATAIN = (0x10 / 4);
	static constexpr uint32_t GPIO_CLEARDATAOUT = (0x0C / 4);
	static constexpr uint32_t GPIO_SETDATAOUT = (0x08 / 4);
#else
	static constexpr uint32_t GPIO_DATAIN = (0x138 / 4);
	static constexpr uint32_t GPIO_CLEARDATAOUT = (0x190 / 4);
	static constexpr uint32_t GPIO_SETDATAOUT = (0x194 / 4);
#endif
	bool shouldUnexport;
	Mmap* mmap = nullptr;
	Pin gpio;
	uint32_t pinMask;
	volatile uint32_t* gpioAddr = nullptr;
};
