/*
Sample sound from I2S microphone, display on Serial Monitor and/or Serial Plotter
Based on https://dronebotworkshop.com/esp32-i2s/ + our infrastructure in audioSTD.h

Test setup:
  - ESP32 (CH9102)
  - Micriphone (INMP441)
  - Arduino IDE 2.3.3
  - Board manager: esp32 v3.0.5 by Espressif Systems

Test scenario:
  1. Upload the sketch and open Serial Plotter.
  2. Press the RESET button on your esp32.
  3. Talk/clap your hands to see how microphone manages to sample your signal.
  4. Press "STOP" button on Serial Plotter, to capture the output.
*/

#include <audioSTD.h>  // from Diana-audio-utils

#define MIC_SAMPLE_RATE (44100)
#define MIC_SAMPLE_BITS (16)  // 16 bits for bit depth
#define I2S_READ_LEN (64)

// Define input buffer
const int bufferLen = I2S_READ_LEN;
int16_t sBuffer[bufferLen];

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);
  Serial.println("Setup I2S ...");

  delay(1000);
  // Init microphone for sampling, using the standard driver
  esp_err_t res = micInitStd(MIC_SAMPLE_RATE, MIC_SAMPLE_BITS, 8, 64, false);
  if (res == ESP_OK) {
    Serial.println("Microphone I2S initialized!");
  }
  delay(500);
}

void loop() {
  // False print statements to "lock range" on serial plotter display
  // Change rangelimit value to adjust "sensitivity"
  int rangelimit = 3000;
  Serial.print(rangelimit * -1);
  Serial.print(" ");
  Serial.print(rangelimit);
  Serial.print(" ");

  // Get I2S data and place in data buffer
  size_t bytes_read = 0;
  //esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytes_read, portMAX_DELAY);
  esp_err_t res = micReadBuff(&sBuffer, bufferLen, &bytes_read);
  if (res == ESP_OK) {
    micPrintSamplesAVG(sBuffer, bytes_read);
  }
}