/*
Sample sound from I2S microphone, write to WAV file on local storage
Built according to video guide by ThatProject: https://www.youtube.com/watch?v=qmruNKeIN-o
Our infrastructure encapsulates the common functionality for FileSystem and Microphone sampling

Test setup:
  - ESP32 (CH9102)
  - Micriphone (INMP441)
  - Arduino IDE 2.3.3
  - Board manager: esp32 v3.0.5 by Espressif Systems

Test scenario: 
  Option 1: Output to serial monitor the buffer content while recording WAV file. 
    Use fsPrintBuffer function during the recording process.
  Option 2: Record WAV to local FS, then read the file and output its content to serial monitor.
    Use fsPrintFileContent function after done recording.
  
  1. Upload the sketch and open Serial Monitor.
  2. Press the RESET button on your esp32.
  3. Play some music and watch the monitor for debugging.
  4. Run Examples->WebServer->FSBrowser, replace the WiFi credentials to your own.
  5. Download the recording.wav file to your computer, and listen to audio for “debugging”.
*/

#define FS_TYPE SPIFFS  // declare another type for testing, e.g. LittleFS
//#define FS_TYPE LittleFS // ERROR: 'LittleFS' was not declared in this scope
#define FS_FORMAT false   // You only need to format the filesystem once
#define MONITORING false  // set true to print for debugging

#include <esp_task_wdt.h>
#include <fsFLASH.h>   // from Diana-audio-utils
#include <audioSTD.h>  // from Diana-audio-utils

#define MIC_SAMPLE_RATE (16000)  // 16kHz
#define MIC_SAMPLE_BITS (16)     // 16 bits for bit depth
#define MIC_CHANNEL_NUM (1)      // one channel
#define I2S_READ_LEN (16 * 1024)
// Ready to have a few tests, 10 secs is quite enough
//#define RECORD_TIME (10)  // seconds
#define RECORD_TIME (20)  // seconds
#define FLASH_RECORD_SIZE (MIC_CHANNEL_NUM * MIC_SAMPLE_RATE * MIC_SAMPLE_BITS / 8 * RECORD_TIME)

// Microphone task definitions
#define MIC_I2S_TASK_STACK (4 * 1024)  // The size of the stack on this TASK is 4096 = 1024*4
#define MIC_I2S_TASK_PRIORITY (5)      // Keep the priority 5 for now.

File file_out;  // holds the recent uploaded file
const char filename_out[] = "/recording.wav";

// got endless list of errors [task_wdt: esp_task_wdt_reset(763): task not found]
// partial workaround
// after enlarging the stack to 4096, looks good
void espTaskInit() {
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = 60000,
    .idle_core_mask = 0,  // i.e. do not watch any idle task
    .trigger_panic = true
  };
  //esp_err_t err = esp_task_wdt_reconfigure(&config);
  esp_task_wdt_deinit();            //wdt is enabled by default, so we need to deinit it first
  esp_task_wdt_init(&twdt_config);  //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);           //add current thread to WDT watch
}

// This i2s_adc_dac is working on a TASK.
// It should be running continuously without return anything.
// If you have a case that you need to turn off, the you have to delete the working TASK by yourself.
// Here, this is a one-time record for now.
// After finishing the recording, the TASK should be done by vTaskDelete(NULL).
void recordingTask(void* arg) {
  int i2s_read_len = I2S_READ_LEN;
  int flash_wr_size = 0;
  size_t bytes_read;

  char* i2s_read_buff = (char*)calloc(i2s_read_len, sizeof(char));
  uint8_t* flash_write_buff = (uint8_t*)calloc(i2s_read_len, sizeof(char));

  // Discard the first two blocks, the microphone may have startup time (i.e. INMP441 up to 83ms)
  Serial.println(" *** Discard the first two blocks *** ");
  micDiscardBlocks((void*)i2s_read_buff, i2s_read_len, &bytes_read, 2);  // audioSTD.h

  Serial.println(" *** Recording Start *** ");
  while (flash_wr_size < FLASH_RECORD_SIZE) {
    // read data from I2S bus, in this case, from ADC.
    micReadBuff((void*)i2s_read_buff, i2s_read_len, &bytes_read);  // audioSTD.h
    if (bytes_read > 0) {
      if (MONITORING) {
        // printing for debugging
        fsPrintBuffer((uint8_t*)i2s_read_buff, 64);  // moved to fsFLASH.h
      }

      // save original data from I2S(ADC) into flash.
      // From the buffer, it's ready to write to file.
      //file_out.write((const byte*)i2s_read_buff, i2s_read_len);

      // Instead of writing directly from the buffer, go through micDataScale and write to the file.
      // This allows to reduce or increase the overall volume including noise.
      micDataScale(flash_write_buff, (uint8_t*)i2s_read_buff, i2s_read_len);
      file_out.write((const byte*)flash_write_buff, i2s_read_len);
      flash_wr_size += i2s_read_len;

      Serial.printf("Sound recording %u%%\n", flash_wr_size * 100 / FLASH_RECORD_SIZE);

      // Check the size of the stack in the TASK.
      // uxTaskGetStackHighWaterMark() shows how much Stack was never used. The closer to 0, the better.
      Serial.printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
      // We confirmed that the size of the Stack doesn't lack or make up a large proportion.
    } else {
      Serial.println("I2S read error");
    }
  }
  Serial.println(" *** Recording Finished *** ");

  // Don't forget to close the file after all done.
  file_out.close();

  free(i2s_read_buff);
  i2s_read_buff = NULL;

  free(flash_write_buff);
  flash_write_buff = NULL;

  // After 10 seconds, the recording was stopped and the file size is 327680 bytes.
  // We know that the wave header is appended to the first part of the file.
  // Hence, the file size should be larger than before, now it's 327724 bytes (+44).

  // re-call listing files
  fsListFiles();

  if (MONITORING) {
    // printing for debugging, using the entire file
    Serial.println(" *** Recording WAV content begin *** ");
    fsPrintFileContent(filename_out);
    Serial.println(" *** Recording WAV content end *** ");
  }

  vTaskDelete(NULL);
}

void startWavRecording() {
  // The "/recording.wav" file starts with this Wave header.
  file_out = FS_TYPE.open(filename_out, FILE_WRITE);
  if (!file_out) {
    Serial.println("File is not available!");
    return;
  }

  // file_out should be open
  byte header[wavHeaderSize];
  fsGenerateWavHeader(header, FLASH_RECORD_SIZE);
  file_out.write(header, wavHeaderSize);

  // Add code for creating a TASK using the FreeRTOS xTaskCreate API.
  // In terms of the DMA Buffer size, need to figure it out soon.
  //xTaskCreate(recordingTask, "MIC I2S Recording", MIC_I2S_TASK_STACK, NULL, MIC_I2S_TASK_PRIORITY, NULL);
  xTaskCreatePinnedToCore(recordingTask, "MIC I2S Recording", MIC_I2S_TASK_STACK, NULL, MIC_I2S_TASK_PRIORITY, NULL, 1);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // got endless list of errors [task_wdt: esp_task_wdt_reset(763): task not found]
  Serial.println("Configuring WDT...");
  espTaskInit();

  Serial.println("Setup I2S...");
  delay(1000);
  // Init microphone for sampling, using the standard driver
  esp_err_t res = micInitStd(MIC_SAMPLE_RATE, MIC_SAMPLE_BITS, 64, 1024, true);
  if (res == ESP_OK) {
    Serial.println("Microphone I2S initialized!");
  }

  Serial.println("Init FS...");
  delay(1000);
  // Init file system for recording, by default use SPIFFS, but can define to use LittleFS
  fsInit();
  // Instead of formatting every time, just removing the previous recording file when it starts.
  fsRemoveFile(filename_out);

  // Before and After recording, check whether the file exists and size.
  fsListFiles();

  Serial.println("Prepare for recording...");
  delay(1000);
  startWavRecording();
}

void loop() {
  // put your main code here, to run repeatedly:
  // got endless list of errors [task_wdt: esp_task_wdt_reset(763): task not found]
  vTaskDelay(portMAX_DELAY);
}

// int i = 0;
// int last = millis();
// void loop() {
//   // resetting WDT every 2s, 5 times only
//   if (millis() - last >= 2000 && i < 5) {
//     Serial.println("Resetting WDT...");
//     esp_task_wdt_reset();
//     last = millis();
//     i++;
//     if (i == 5) {
//       Serial.println("Stopping WDT reset. CPU should reboot in 3s");
//     }
//   }
// }
