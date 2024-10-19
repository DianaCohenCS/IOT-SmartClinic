/**
 * Basic definitions for audio I2S.
 * Wiring the input and output pins
 */

// define microphone pins (input)
#define MIC_I2S_SD 25
#define MIC_I2S_SCK 33
#define MIC_I2S_WS 32

// Use I2S processor 0
#define MIC_I2S_PORT I2S_NUM_0
// LRC is grounded, hence only left channel
#define MIC_CHANNEL_FMT I2S_CHANNEL_FMT_ONLY_LEFT


// define DAC pins (output via speakers)
#define DAC_DIN_SD 14
#define DAC_BCLK_SCK 27
#define DAC_LRC_WS 26

// Use I2S processor 0
#define DAC_I2S_PORT I2S_NUM_0
#define DAC_CHANNEL_FMT I2S_CHANNEL_FMT_ONLY_LEFT
//#define DAC_CHANNEL_FMT I2S_CHANNEL_FMT_RIGHT_LEFT