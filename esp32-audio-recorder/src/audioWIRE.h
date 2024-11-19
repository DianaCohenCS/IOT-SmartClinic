/**
 * Basic definitions for audio I2S.
 * Wiring the input and output pins
 */

// define microphone pins (input)
#define MIC_I2S_SD 25
#define MIC_I2S_SCK 33
#define MIC_I2S_WS 32

// Use I2S processor 0 for receiver
#define MIC_I2S_PORT I2S_NUM_0
// LRC is grounded, hence only left channel
#define MIC_CHANNEL_FMT I2S_CHANNEL_FMT_ONLY_LEFT
//#define MIC_CHANNEL_FMT I2S_CHANNEL_FMT_ONLY_RIGHT

// define DAC pins (output via speakers)
#define DAC_DIN_SD 14
#define DAC_BCLK_SCK 27
#define DAC_LRC_WS 26

// Use I2S processor 0 for transmitter
#define DAC_I2S_PORT I2S_NUM_0
#define DAC_CHANNEL_FMT I2S_CHANNEL_FMT_ONLY_LEFT
//#define DAC_CHANNEL_FMT I2S_CHANNEL_FMT_ONLY_RIGHT
// #define DAC_CHANNEL_FMT I2S_CHANNEL_FMT_RIGHT_LEFT - for stereo

// TODO: for playing WAV - replace with extracted values from the WAV header
#define MIC_SAMPLE_RATE (16000) // 16kHz | 44100
#define MIC_SAMPLE_BITS (16)    // 32 or 16 bits for bit depth // 32-bit doesn't work for me :(
#define MIC_SAMPLE_BITS_HDR (16)    // 16 bits for bit depth for wav header of output file
#define MIC_CHANNEL_NUM (1)     // one channel

// Increase these values if you experience distortion at higher sample rates
#define DMA_BUF_COUNT (8) // 64
#define DMA_BUF_LEN (256)  // 64; 1024

#define BUFF_SIZE (DMA_BUF_LEN * MIC_SAMPLE_BITS / 8) // size of buffer in bytes