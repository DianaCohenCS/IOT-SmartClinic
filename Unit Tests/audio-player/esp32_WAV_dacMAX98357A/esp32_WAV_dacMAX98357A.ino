/*
Sample sound from I2S microphone, write to WAV file on local storage
Built according to video guide by ThatProject: https://www.youtube.com/watch?v=qmruNKeIN-o
Our infrastructure encapsulates the common functionality for FileSystem and Microphone sampling

Test setup:
  - ESP32 (CH9102)
  - Speaker with DAC (MAX98357A)
  - Arduino IDE 2.3.3
  - Board manager: esp32 v3.0.5 by Espressif Systems

Test scenario: 
  Play the WAV file, previously recorded to local FS (flash)
  
  1. Upload the sketch and open Serial Monitor.
  2. Press the RESET button on your esp32.
  3. Play the WAV file from the local FS.
*/

#include <fsFLASH.h>  // from Diana-audio-utils
#include <audioSTD.h> // from Diana-audio-utils

#define MIC_SAMPLE_RATE (16000)  // 16kHz
#define MIC_SAMPLE_BITS (16)     // 16 bits for bit depth
#define MIC_CHANNEL_NUM (1)      // one channel
//#define I2S_READ_LEN (16 * 1024)

const char filename_in[] = "/recording.wav";

void playWavRecording(const char* filename) {
  File audioFile = FS_TYPE.open(filename);
  if (!audioFile) {
    Serial.println("Failed to open audio file");
    return;
  }

  // Validate WAV header
  if (!fsEnsureWavHeader(audioFile)) {
    Serial.println("Failed to validate WAV header");
    audioFile.close();
    return;
  }

  // Buffer to hold audio data
  uint8_t buffer[64];
  size_t bytesRead;

  while (audioFile.available()) {
    bytesRead = audioFile.read(buffer, sizeof(buffer));
    size_t bytesWritten;
    //i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
    dacWriteBuff(buffer, bytesRead, &bytesWritten); // audioSTD.h
  }

  audioFile.close(); 
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.println("Setup I2S...");
  delay(1000);
  // Init DAC for speakers, using the standard driver
  esp_err_t res = dacInitStd(MIC_SAMPLE_RATE, MIC_SAMPLE_BITS, 64, 1024, false);
  if (res == ESP_OK) {
    Serial.println("DAC I2S initialized!");
  }

  Serial.println("Init FS...");
  delay(1000);
  // Init file system for recording, by default use SPIFFS, but can define to use LittleFS
  fsInit();

  // Before and After recording, check whether the file exists and size.
  fsListFiles();

  Serial.println("Prepare for playing audio...");
  delay(1000);

  // play the file
  Serial.println(" *** Play WAV Start *** ");
  playWavRecording(filename_in);
  Serial.println(" *** Play WAV Finished *** ");
}

void loop() {
  // put your main code here, to run repeatedly:
}