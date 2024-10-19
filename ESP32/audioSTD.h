/**
 * Audio input and output using the standard built-in driver for I2S
 */

// For PlatformIO need to begin with this include
//#include <Arduino.h>

// this is a built-in espressif driver for I2S, no additional library for sound
#include <driver/i2s.h>

#include "audioWIRE.h" // get the constants of audio wiring

/**
 * Input for recording/sampling audio
 */

// install and start the standard I2S driver, using driver/i2s.h
esp_err_t micInstallDriverStd(uint32_t sampleRate, int bitsPerSample, int bufCount, int bufLen, bool useApll) {
  // setup I2S processor configuration
  i2s_config_t mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),  // MASTER - controller mode, RX - receiver
    .sample_rate = sampleRate,
    .bits_per_sample = i2s_bits_per_sample_t(bitsPerSample),
    .channel_format = MIC_CHANNEL_FMT,
    //.communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB), // deprecated
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S), // use this one instead, if fail to compile, then switch to an old version line above
    .intr_alloc_flags = 0, // default interrupt priority
    .dma_buf_count = bufCount, // number of buffers, deprecated. use dma_desc_num - number of descriptors used by I2S DMA
    .dma_buf_len = bufLen, // samples per buffer, deprecated. use - dma_frame_num frame number for one-time sampling
    .use_apll = useApll // I2S using APLL as main I2S clock, enable it to get accurate clock
  };

  // install and start I2S driver, i2s_queue=NULL driver will not use an event queue
  return i2s_driver_install(MIC_I2S_PORT, &mic_config, 0, NULL);
}

esp_err_t micSetPinsStd() {
  // Set I2S pin configuration
  const i2s_pin_config_t mic_pin_config = {
    .bck_io_num = MIC_I2S_SCK,
    .ws_io_num = MIC_I2S_WS,
    .data_out_num = -1,
    .data_in_num = MIC_I2S_SD
  };

  return i2s_set_pin(MIC_I2S_PORT, &mic_pin_config);
}

// init the microphone, using the driver/i2s.h
esp_err_t micInitStd(uint32_t sampleRate, int bitsPerSample, int bufCount, int bufLen, bool useApll) {
  // install and start I2S driver, i2s_queue=NULL driver will not use an event queue
  esp_err_t res = micInstallDriverStd(sampleRate, bitsPerSample, bufCount, bufLen, useApll);
  if (res != ESP_OK) {
    //Serial.printf("Microphone install driver failed with error 0x%x", res);
    printf("Microphone install driver failed with error 0x%x\n", res);
    return res;
  }

  res = micSetPinsStd();
  if (res != ESP_OK) {
    //Serial.printf("Microphone set pin configuration failed with error 0x%x", res);
    printf("Microphone set pin configuration failed with error 0x%x\n", res);
    return res;
  }

  // It is not necessary to call this function after i2s_driver_install()(it is started automatically), 
  // however it is necessary to call it after i2s_stop().
  res = i2s_start(MIC_I2S_PORT);
  if (res != ESP_OK) {
    //Serial.printf("Microphone start I2S driver failed with error 0x%x", res);
    printf("Microphone start I2S driver failed with error 0x%x\n", res);
    return res;
  }
  return ESP_OK;  // success
}

esp_err_t micReadBuff(void* dest, size_t size, size_t* bytes_read) {
  return i2s_read(MIC_I2S_PORT, dest, size, bytes_read, portMAX_DELAY);
}

void micDiscardBlocks(void* dest, size_t size, size_t* bytes_read, int num_blocks) {
  for (int i = 0; i < num_blocks; i++)
    micReadBuff(dest, size, bytes_read);
}

// Scale data to 8bit for data from ADC.
// Data from ADC are 12bit width by default.
// DAC can only output 8 bit data.
// Scale each 12bit ADC data to 8bit DAC data.
void micDataScale(uint8_t* d_buff, uint8_t* s_buff, uint32_t len) {
  uint32_t j = 0;
  uint32_t dac_value = 0;
  for (int i = 0; i < len; i += 2) {
    dac_value = ((((uint16_t)(s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
    d_buff[j++] = 0;
    // The bigger the stored value, it makes the louder the sound and noise as well.
    // The default is dac_value * 256 / 4096 is a way to keep less noise and a little bit up the volume.
    // In our case, we want to get almost the same volume so we use 2048 instead of 4096.
    d_buff[j++] = dac_value * 256 / 2048;
  }
}

// Print Average reading to serial plotter
void micPrintSamplesAVG(int16_t src[], size_t len) {
  // Read I2S data buffer
  int16_t samples_read = len / 8;
  if (samples_read > 0) {
    float mean = 0;
    for (int16_t i = 0; i < samples_read; ++i) {
      mean += src[i];
    }

    // Average the data reading
    mean /= samples_read;
    
    // Print to serial plotter
    printf("%.2f\n", mean);
  }
}



/**
 * Output for playing audio
 */

// install and start the standard I2S driver, using driver/i2s.h
esp_err_t dacInstallDriverStd(uint32_t sampleRate, int bitsPerSample, int bufCount, int bufLen, bool useApll) {
  // setup I2S processor configuration
  i2s_config_t dac_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // MASTER - controller mode, TX - transmitter
      .sample_rate = sampleRate,
      .bits_per_sample = i2s_bits_per_sample_t(bitsPerSample),
      .channel_format = DAC_CHANNEL_FMT,
      //.communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB), // deprecated
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S), // use this one instead, if fail to compile, then switch to an old version line above
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                             // from an example: ESP_INTR_FLAG_LEVEL1, from mic_config: 0,
      .dma_buf_count = bufCount,                                            // number of buffers, deprecated. use dma_desc_num - number of descriptors used by I2S DMA
      .dma_buf_len = bufLen,                                                // samples per buffer, deprecated. use - dma_frame_num frame number for one-time sampling
      .use_apll = useApll,                                                  // I2S using APLL as main I2S clock, enable it to get accurate clock
      .tx_desc_auto_clear = true};

  // install and start I2S driver, i2s_queue=NULL driver will not use an event queue
  return i2s_driver_install(DAC_I2S_PORT, &dac_config, 0, NULL);
}

esp_err_t dacSetPinsStd() {
  // Set I2S pin configuration
  const i2s_pin_config_t dac_pin_config = {
    .bck_io_num = DAC_BCLK_SCK,
    .ws_io_num = DAC_LRC_WS,
    .data_out_num = DAC_DIN_SD,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  return i2s_set_pin(DAC_I2S_PORT, &dac_pin_config);
}

// init the speaker (DAC), using the driver/i2s.h
esp_err_t dacInitStd(uint32_t sampleRate, int bitsPerSample, int bufCount, int bufLen, bool useApll) {
  // install and start I2S driver, i2s_queue=NULL driver will not use an event queue
  esp_err_t res = dacInstallDriverStd(sampleRate, bitsPerSample, bufCount, bufLen, useApll);
  if (res != ESP_OK) {
    //Serial.printf("DAC install driver failed with error 0x%x", res);
    printf("DAC install driver failed with error 0x%x\n", res);
    return res;
  }

  res = dacSetPinsStd();
  if (res != ESP_OK) {
    //Serial.printf("DAC set pin configuration failed with error 0x%x", res);
    printf("DAC set pin configuration failed with error 0x%x\n", res);
    return res;
  }

  res = i2s_set_clk(DAC_I2S_PORT, sampleRate, bitsPerSample, I2S_CHANNEL_MONO);
  if (res != ESP_OK) {
    //Serial.printf("DAC set clock failed with error 0x%x", res);
    printf("DAC set clock failed with error 0x%x\n", res);
    return res;
  }

  return ESP_OK;  // success
}

esp_err_t dacWriteBuff(void* src, size_t size, size_t* bytes_written) {
  return i2s_write(DAC_I2S_PORT, src, size, bytes_written, portMAX_DELAY);
}

