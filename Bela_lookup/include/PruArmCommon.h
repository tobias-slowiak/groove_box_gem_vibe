#ifndef PRU_ARM_COMMON_H
#define PRU_ARM_COMMON_H

#define USE_MCASP // enable this to actually use audio

// this file is included by both core/Pru.cpp and pru/pru_rtaudio*.p
#define BOARD_FLAGS_POCKET_BEAGLE 0
#define BOARD_FLAGS_BELA_GENERIC_TDM 3
#define BOARD_FLAGS_SHOULD_SKIP_DAC 4

#define PRU_SYSTEM_EVENT_RTDM 20

// error codes sent from the PRU
#define ARM_ERROR_TIMEOUT 1
#define ARM_ERROR_XUNDRUN 2
#define ARM_ERROR_XSYNCERR 3
#define ARM_ERROR_XCKFAIL 4
#define ARM_ERROR_XDMAERR 5
#define ARM_ERROR_ROVRN ARM_ERROR_XUNDRUN + 4
#define ARM_ERROR_RSYNCERR ARM_ERROR_XSYNCERR + 4
#define ARM_ERROR_RCKFAIL ARM_ERROR_XCKFAIL + 4
#define ARM_ERROR_RDMAERR ARM_ERROR_XDMAERR + 4
#define ARM_ERROR_INVALID_INIT 10
#define ARM_ERROR_OVERFLOW_MCASP_ADC 11
#define ARM_ERROR_OVERFLOW_MCASP_DAC 12
#define ARM_ERROR_OVERFLOW_MCSPI_ADC 13
#define ARM_ERROR_OVERFLOW_MCSPI_DAC 14
#define ARM_ERROR_OVERFLOW_DIGITAL 15
#define ARM_ERROR_SPI_NOT_DONE 16

// Offsets within CPU <-> PRU communication memory (4 byte slots)
#define COMM_SHOULD_STOP                 0 // Set to be nonzero when loop should stop
#define COMM_CURRENT_BUFFER              4 // Which buffer we are on
#define COMM_BUFFER_MCASP_FRAMES         8 // How many frames per buffer for audio
#define COMM_SPI_CONFIG                 12 // Configuration of SPI acquisition (see PRU.cpp for struct details)
#define COMM_LED_ADDRESS                24 // Which memory address to find the status LED on
#define COMM_LED_PIN_MASK               28 // Which pin to write to change LED
#define COMM_FRAME_COUNT                32 // How many frames have elapse since beginning
#define COMM_USE_DIGITAL                44 // Whether or not to use DIGITAL
#define COMM_PRU_NUMBER                 48 // Which PRU this code is running on
#define COMM_MUX_CONFIG                 52 // Whether to use the mux capelet, and how many channels
#define COMM_MUX_END_CHANNEL            56 // Which mux channel the last buffer ended on
#define COMM_BUFFER_SPI_FRAMES          60 // How many frames per buffer for analog i/o
#define COMM_BOARD_FLAGS                64 // Flags for the board we are on (BOARD_FLAGS_... are defined in include/PruArmCommon.h)
#define COMM_ERROR_OCCURRED             68 // Signals the ARM CPU that an error happened
#define COMM_ACTIVE_CHANNELS            72 // How many TDM slots contain useful data
// the order of the following registers has to strictly follow the order of the
// members of McaspRegisters
#define COMM_MCASP_CONF_START           76
#define COMM_MCASP_OUT_CHANNELS         (COMM_MCASP_CONF_START+0) // 2 bytes
#define COMM_MCASP_IN_CHANNELS          (COMM_MCASP_CONF_START+2) // 2 bytes
#define COMM_MCASP_OUT_SERIALIZERS_DISABLED_SUBSLOTS (COMM_MCASP_CONF_START+4) // 4 bytes, bitmask for 32 subslots: when it's high, send dummy data for this subslot
#define COMM_MCASP_IN_SERIALIZERS_DISABLED_SUBSLOTS  (COMM_MCASP_CONF_START+8)

// ARM accesses these memory locations as uint32_t
// to avoid duplication and mistakes, we use macros to generate the values for ARM
// pasm is stupid and doesn't know about __TIME__, so this gives us a clue that
// we are using clang/gcc and we can use some more advanced preprocessor
// directives in here to generate some C code
#ifdef __TIME__
#define ENUM(NAME) PRU_ ## NAME = (NAME/4),
typedef enum {
ENUM(COMM_SHOULD_STOP)
ENUM(COMM_CURRENT_BUFFER)
ENUM(COMM_BUFFER_MCASP_FRAMES)
ENUM(COMM_LED_ADDRESS)
ENUM(COMM_LED_PIN_MASK)
ENUM(COMM_FRAME_COUNT)
ENUM(COMM_SPI_CONFIG)
ENUM(COMM_USE_DIGITAL)
ENUM(COMM_PRU_NUMBER)
ENUM(COMM_MUX_CONFIG)
ENUM(COMM_MUX_END_CHANNEL)
ENUM(COMM_BUFFER_SPI_FRAMES)
ENUM(COMM_BOARD_FLAGS)
ENUM(COMM_ERROR_OCCURRED)
ENUM(COMM_ACTIVE_CHANNELS)
ENUM(COMM_MCASP_CONF_START)
} PruCommonFlags;
#endif // __TIME__

#endif /* PRU_ARM_COMMON_H */
