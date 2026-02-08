#pragma once
#include <Gpio.h>
#if defined(IS_AM62_SK)
// This assumes SK-AM62x
const unsigned int codecI2cBus = 1; // Bus for TLV320AIC3106 codec
// TODO: update the rest of these
const unsigned int kBelaCapeButtonPin = -1; // unused
const unsigned int kAmplifierMutePin = -1; // unused
const unsigned int kSpiDacChipSelectPin = 5; // P9.17 used for ADC on BelaMini
const unsigned int kSpiAdcChipSelectPin = 48; // P9.15 unused on BelaMini
// below are for user LED USR3
const unsigned int kUserLedGpioPin = 49;    // GPIO49 for USR LED1 on SK-AM62x
const unsigned int kUserLedNumber = 1;      // TODO: check this
const char kUserLedDefaultTrigger[] = "heartbeat";
#elif defined (IS_AM62_PB2)
// PocketBeagle 2
const unsigned int codecI2cBus = 2;
const Gpio::Pin kBelaCapeButtonPin {0, 47}; // P2.06
const Gpio::Pin kAmplifierMutePin; // Unused
const Gpio::Pin kSpiDacChipSelectPin {1, 11}; // P2.01, GPIO1_11 (this also has MCSPI2_CS1)
const Gpio::Pin kSpiAdcChipSelectPin {1, 13}; // P1.06, GPIO1_13 (this also has MCSPI2_CS0)
const Gpio::Pin kUserLedGpioPin = {0, 3}; // LED USER 4, GPIO0_4, ball E25
const unsigned int kUserLedNumber = 4; // LED USER 4
const char kUserLedDefaultTrigger[] = "none";
#elif defined(IS_AM62_BP) || defined (IS_AM62_PB2) // together for now
// BeaglePlay
const unsigned int codecI2cBus = 1; // Bus for TLV320AIC3104 codec via Grove connector loopback
const unsigned int kBelaCapeButtonPin = 118; // GPIO1.24 -- RX on Mikrobus header
const unsigned int kAmplifierMutePin = -1; // Unused
const unsigned int kSpiDacChipSelectPin = -1; // Unused
const unsigned int kSpiAdcChipSelectPin = 107; // GPIO1.13 -- CS on Mikrobus header
const unsigned int kUserLedGpioPin = 57; // DUMMY VALUE to make it work for GPIO1.25
const unsigned int kUserLedNumber = 3; // DUMMY VALUE: even when addressing USR3 on BeaglePlay I can't set it
const char kUserLedDefaultTrigger[] = "mmc1";
#else
// This assumes BBB/BBG/PB
const unsigned int codecI2cBus = 2; // Bus for TLV320AIC3104 codec
const Gpio::Pin kBelaCapeButtonPin {0, 115}; //P9.27 / P2.34
const Gpio::Pin kAmplifierMutePin {0, 61}; // P8.26 / nothing on BelaMini
const Gpio::Pin kSpiDacChipSelectPin {0, 5}; // P9.17 used for ADC on BelaMini
const Gpio::Pin kSpiAdcChipSelectPin {0, 48}; // P9.15 unused on BelaMini
// below are for user LED USR3
const Gpio::Pin kUserLedGpioPin {0, 56};
const unsigned int kUserLedNumber = 3;
const char kUserLedDefaultTrigger[] = "mmc1";
#endif

#if defined(IS_AM62_SK)
const unsigned int tlv320CodecI2cAddress = 0x1b; // Address of TLV320AIC3106 codec on SK-AM62x
#else
const unsigned int tlv320CodecI2cAddress = 0x18; // Address of TLV320AIC3104
#endif
const unsigned int es9080CodecAddress = 0x4c; // write-only address of TLV320AIC3104
#if defined(IS_AM62_PB2)
const Gpio::Pin es9080CodecResetPin = {0, 53}; // reset GPIO
#else
const Gpio::Pin es9080CodecResetPin = {0, 11}; // reset GPIO
#endif
#ifdef GEMMULTI_REVA1
const unsigned int adc3140CodecI2cAddress1 = 0x4c;	// for Gem Multi rev. A1
const unsigned int adc3140CodecI2cAddress2 = 0x4e;	// for Gem Multi rev. A1
#else
const unsigned int adc3140CodecI2cAddress1 = 0x4e;	// for Gem Multi rev. A2
const unsigned int adc3140CodecI2cAddress2 = 0x4f;	// for Gem Multi rev. A2
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 108) // first kernel we shipped with a different location of the spidevs
const char ctagSpidevGpioCs0[] = "/dev/spidev3.0"; // Path for SPI bus 0
const char ctagSpidevGpioCs1[] = "/dev/spidev3.1"; // Path for SPI bus 1
#else // 4.14.108
const char ctagSpidevGpioCs0[] = "/dev/spidev32766.0"; // Path for SPI bus 0
const char ctagSpidevGpioCs1[] = "/dev/spidev32766.1"; // Path for SPI bus 1
#endif // 4.14.108
