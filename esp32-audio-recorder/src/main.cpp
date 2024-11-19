/*
Audio Recorder on-board (esp32) web server.
Display the playlist (FS content).
Upload audio files (MP3, WAV in different qualities) to local storage (flash).
Play any file, through the GUI.
Record your audio from Microphone through GUI – directly to local storage (flash).
Ability to play the last recording, through the GUI.
Ability to delete any audio file?
Our infrastructure encapsulates the common functionality for FileSystem and Microphone sampling

Test setup:
  - ESP32 (CH9102)
  - Micriphone (INMP441)
  - Speaker attached to DAC (MAX98357A)
  - Arduino IDE 2.3.3 / PlatformIO on VSCodium
  - Board manager: esp32 v3.0.6 by Espressif Systems

Test scenario:
  Play the WAV file, previously recorded to local FS (flash)

  TODO: replace...
  1. Upload the sketch and open Serial Monitor.
  2. Press the RESET button on your esp32.
  3. Play the WAV file from the local FS.
*/

// Import the required libraries
#include <Arduino.h> // for PlatformIO

#include <ESPAsyncWebServer.h> // includes AsyncTCP and WiFi when ESP32 is defined
#include <ESPmDNS.h>           // to find our device via host-name
#include <AsyncJson.h>         // let's try sending messages in json format

// #include <semphr.h> // limit concurrent requests

// our definitions and wrappers from Diana-audio-utils
#include "secrets.h"
#include "fsFLASH.h"
#include "audioSTD.h"
#include <esp_wpa2.h>

#define LED LED_BUILTIN

// Microphone task definitions
#define MIC_I2S_TASK_STACK (4 * 1024) // The size of the stack on this TASK is 4096 = 1024*4
#define MIC_I2S_TASK_PRIORITY (1)     // Keep the priority 5 for now.
#define DAC_I2S_TASK_STACK (4 * 1024)
#define DAC_I2S_TASK_PRIORITY (1)

File file_out;        // holds the recent uploaded file
int record_time = 20; // seconds

// File path can be 31 characters maximum in SPIFFS
String audio_dir = "/";
String filename_out = audio_dir + "recording.wav";
String filename_in = audio_dir + "recording_spiffs.wav";

// Replace with your network credentials, defined in secrets.h
// #define EAP_WIFI false
// const char *ssid_eap = EAP_SSID;
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

const char *host = "audio-recorder";
int counter = 0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// PLAY: task for playing the audio, when done, backs to NULL
TaskHandle_t playbackTaskHandle = NULL;
TaskHandle_t recordingTaskHandle = NULL;
// since the play is on esp itself, we allow only one at a time
SemaphoreHandle_t audioMutex;
// implement a semaphore to limit the number of concurrent uploads
SemaphoreHandle_t uploadSemaphore;

// put function declarations here:

// each POST request has 3 handlers - onRequest, onUpload and onBody
void onUpload(AsyncWebServerRequest *, String, size_t, uint8_t *, size_t, bool);
void handleRecordingRequest(AsyncWebServerRequest *);
void handlePlayRequest(AsyncWebServerRequest *);
void handleDeleteRequest(AsyncWebServerRequest *);

String getAudioPath(String);
String extractParam(AsyncWebServerRequest *, String, bool);
String extractFilePath(AsyncWebServerRequest *);
unsigned long getFlashRecordSize();

void playingTask(void *);
void playWavRecording(String);
void recordingTask(void *);
bool prepareForRecording();
unsigned long recordWav();  // based on Tom's code
unsigned long _recordWav(); // original
void cleanupRecording(bool, bool);

void setup()
{
  // put your setup code here, to run once:
  pinMode(LED, OUTPUT);

  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println("\nBooted");
  digitalWrite(LED, HIGH);

  // FS INIT
  Serial.println("\nInit FS...");
  // delay(1000);
  // Init file system for recording, by default use SPIFFS, but can define to use LittleFS
  fsInit();
  Serial.println("FS mounted");
  // Before and After recording, check whether the file exists and size.
  fsListFiles();

  // SETUP DAC and MIC on demand
  Serial.println("Init I2S...");
  // delay(1000);
  Serial.println("I2S will we configured on demand only");

  // WIFI INIT
  Serial.println("\nInit WiFi...");
  // delay(1000);
  WiFi.disconnect(true); // disconnect from WiFi to set new WiFi connection
  WiFi.mode(WIFI_STA);   // init wifi mode as station

  Serial.print("Connecting to ");
  // if (EAP_WIFI) // this didn't work for me...
  // {
  //   Serial.print(ssid_eap);

  //   // WPA2 enterprise magic starts here

  //   // esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ANONYMOUS_IDENTITY, strlen(EAP_ANONYMOUS_IDENTITY));
  //   // esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
  //   // esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));

  //   // esp_wifi_sta_wpa2_ent_enable();
  //   // // esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
  //   // // esp_wifi_sta_wpa2_ent_enable(&config);

  //   // WPA2 enterprise magic ends here

  //   // WiFi.begin(ssid_eap);
  //   WiFi.begin(ssid_eap, WPA2_AUTH_PEAP, EAP_ANONYMOUS_IDENTITY, EAP_USERNAME, EAP_PASSWORD); // without CERTIFICATE, RADIUS server EXCEPTION "for old devices" required
  // }
  // else
  {
    Serial.print(ssid);
    WiFi.begin(ssid, password);
  }
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
    counter++;
    if (counter >= 60)
    { // after 60 seconds timeout - reset board
      ESP.restart();
    }
  }
  // connected
  Serial.println("\nWiFi connected!");
  Serial.print("Local IP address: ");
  Serial.println(WiFi.localIP());

  // DNS INIT
  Serial.println("\nInit local DNS...");
  // delay(1000);
  MDNS.begin(host);
  Serial.printf("Browse either http://%s.local or via IP address\n", host);
  Serial.println("* The .local extension is essential when using the host-name.");

  // WEB SERVER SETUP
  Serial.println("\nInit local web server...");
  // delay(1000);
  uploadSemaphore = xSemaphoreCreateCounting(3, 3); // Allow up to 3 concurrent uploads
  audioMutex = xSemaphoreCreateMutex();

  // CORS handlers
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  // REFRESH on redirect
  // DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  // DefaultHeaders::Instance().addHeader("Pragma", "no-cache");
  // DefaultHeaders::Instance().addHeader("Expires", "0");

  // DEFINE ENDPOINTS
  // define "led" endpoint, to get the json input
  server.addHandler(new AsyncCallbackJsonWebHandler("/led", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                    {
    const JsonObject &jsonObj = json.as<JsonObject>();
    if (jsonObj["on"])
    {
      Serial.println("Turn on LED");
      digitalWrite(LED, HIGH);
    }
    else
    {
      Serial.println("Turn off LED");
      digitalWrite(LED, LOW);
    }
    request->send(200, "OK"); })); // don't forget to send the response

  // serve content from SPIFFS, we flush html content every time by setting no cache
  server.serveStatic("/", FS_TYPE, "/", "max-age=0").setDefaultFile("index.html");

  // Route to load style.css file, index.html loads this
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(FS_TYPE, "/style.css", "text/css"); });

  // Route to load script.js file, index.html loads this
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(FS_TYPE, "/script.js", "text/javascript"); });

  // Route to populate the available FS space
  server.on("/space", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String space = fsGetSpace();
    request->send(200, "application/json", space); });

  // Route to populate the playlist from FS
  server.on("/playlist", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String playlist = fsGetPlaylist(); // don't pass any dir, this must stay the root
    request->send(200, "application/json", playlist); });

  // Route to upload (multiple) files
  // each POST consists of onRequest, onUpload and onBody handlers
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
            { request->send(200); }, onUpload);

  // Route to delete a single audio file, the filename must be provided
  // allow to delete audio files ONLY
  server.on("/edit", HTTP_DELETE, handleDeleteRequest);

  // Route to play on ESP using DAC module
  // we support only WAV files here, the filename must be provided
  server.on("/play", HTTP_POST, handlePlayRequest);

  // Route to record WAV file via a microphone attached to ESP
  server.on("/record", HTTP_POST, handleRecordingRequest);

  // Route to get the i2s status,
  // whether currently busy (playing/recording) or ready to accept the task
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String status = (playbackTaskHandle != NULL || recordingTaskHandle != NULL) ? "busy" : "ready";
    request->send(200, "text/plain", status); });

  // // play in browser directly
  // server.on(filename_in.c_str(), HTTP_GET, [](AsyncWebServerRequest *request)
  //           {
  //   // ensure this is an audio file
  //   if (!filename_in.endsWith(".wav") && !filename_in.endsWith(".mp3") || !fsExists(filename_in)) {
  //     Serial.printf("%s\n not found", filename_in);
  //     request->send(404, "Not found");
  //   }
  //   else{
  //     request->send(FS_TYPE, filename_in, "audio/wav");
  //   } });

  // handle not found, except for OPTIONS request
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    if (request->method() == HTTP_OPTIONS) // preflight request
    {
      request->send(200);
    }
    else
    {
      Serial.println("Not found");
      request->send(404, "text/plain", "Not found");
    } });

  // START WEB SERVER
  server.begin();
  Serial.println("WebServer started\n");
  digitalWrite(LED, LOW);
}

void loop()
{
  // // put your main code here, to run repeatedly:
  // if (WiFi.status() == WL_CONNECTED)
  // {              // if we are connected to Eduroam network
  //   counter = 0; // reset counter
  //   Serial.println("Wifi is still connected with IP: ");
  //   Serial.println(WiFi.localIP()); // inform user about his IP address
  // }
  // else if (WiFi.status() != WL_CONNECTED)
  // { // if we lost connection, retry
  //   WiFi.begin(ssid);
  // }
  // while (WiFi.status() != WL_CONNECTED)
  // { // during lost connection, print dots
  //   delay(1000);
  //   Serial.print(".");
  //   counter++;
  //   if (counter >= 60)
  //   { // 30 seconds timeout - reset board
  //     ESP.restart();
  //   }
  // }

  // test LED
  // digitalWrite(LED, LOW);
  // delay(1000);
  // digitalWrite(LED, HIGH);
  // delay(1000);
}

// put function definitions here:

// Replaces placeholder with LED state value
// String processor(const String &var)
// {
//   Serial.printf("%s: ", var);
//   if (var == "STATE")
//   {
//     if (digitalRead(LED))
//     {
//       ledState = "ON";
//     }
//     else
//     {
//       ledState = "OFF";
//     }
//     Serial.println(ledState);
//     return ledState;
//   }
//   return String();
// }

// handle files upload in chunks
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  // The upload is called for all the selected files in sequence.
  if (!index) // Start of upload
  {
    // Serial.println("Obtaining the Semaphore...");
    if (xSemaphoreTake(uploadSemaphore, 0) != pdTRUE)
    {
      request->send(429, "text/plain", "Too many concurrent uploads.");
      return;
    }
    // Serial.println("Semaphore was obtained!");

    if (fsAvailableSpace() < 1024)
    {
      Serial.println("Warning: Less than 1KB space is available.");
    }
    // print available heap
    // Serial.printf("free_heap: %u\n", ESP.getFreeHeap());

    // Open the file on first call and store the handle in the request object
    digitalWrite(LED, HIGH); // working...
    Serial.printf("Upload Start: %s\n", filename.c_str());

    String path = getAudioPath(filename);
    // bool hasErrors = false;
    if (!path.endsWith(".wav") && !path.endsWith(".mp3"))
    {
      Serial.println("Unsupported file extension.");
      // hasErrors = true;
      return;
    }

    // File path can be 31 characters maximum in SPIFFS, including the "/" prefix
    if (path.length() > 31)
    {
      Serial.println("File name is too long.");
      // hasErrors = true;
      return;
    }

    // if (hasErrors) // perhaps the release is in (final) case - end of upload
    // {
    //   xSemaphoreGive(uploadSemaphore); // relese the lock
    //   return;
    // }

    // ensure to remove the file, if exists
    fsRemoveFile(path);

    request->_tempFile = FS_TYPE.open(path, FILE_WRITE);
  }

  if (len) // Write the received bytes to the file
  {
    if (fsAvailableSpace() < len)
    {
      Serial.println("Error: Not enough space.");
      request->_tempFile.close();
      // request->send(507, "text/plain", "Insufficient Storage for " + filename);
      return;
    }
    else
    {
      // Stream the incoming chunk to the opened file
      request->_tempFile.write(data, len);
    }
  }

  if (final) // End of upload
  {
    // index + len
    Serial.printf("Upload End: %s, %u B\n", filename.c_str(), request->_tempFile.size());
    // Close the file handle as the upload is now done
    request->_tempFile.close();

    // IMPORTANT:
    // don't 'request->send' here, otherwise the response is sent before the last file is uploaded

    // instead of redirect here, we redirect on the client side, after all the files have beed uploaded
    // request->redirect("/");

    // Serial.println("Releasing the Semaphore...");
    digitalWrite(LED, LOW); // done...
    xSemaphoreGive(uploadSemaphore);
    // Serial.println("Semaphore released!\n");
  }
}

void handleDeleteRequest(AsyncWebServerRequest *request)
{
  Serial.println("Prepare for deleting audio file...");

  String path = extractFilePath(request);
  if (path.isEmpty())
    return;

  if (!path.endsWith(".wav") && !path.endsWith(".mp3"))
  {
    // 415 Unsupported Media Type
    request->send(415, "text/plain", "Cannot delete " + path + " due to unsupported extension");
    return;
  }

  if (fsRemoveFile(path))
    request->send(200, "text/plain", "Removed from FS");
  else
    request->send(500, "text/plain", "Failed to delete " + path);
}

void handlePlayRequest(AsyncWebServerRequest *request)
{
  Serial.println("Prepare for playing audio...");

  // extract the filename from the query string
  String path = extractFilePath(request);
  if (path.isEmpty())
    return;

  if (!path.endsWith(".wav"))
  {
    // 415 Unsupported Media Type
    request->send(415, "text/plain", "Cannot play " + path + " due to unsupported extension");
    return;
  }

  if (playbackTaskHandle == NULL && recordingTaskHandle == NULL)
  {
    // xTaskCreate(playingTask, "Play WAV", DAC_I2S_TASK_STACK, (void *)path.c_str(), DAC_I2S_TASK_PRIORITY, &playbackTaskHandle);
    xTaskCreatePinnedToCore(playingTask, "Play WAV", DAC_I2S_TASK_STACK, (void *)path.c_str(), DAC_I2S_TASK_PRIORITY, &playbackTaskHandle, 1);
    request->send(200, "text/plain", "Playing " + path);
  }
  else
  {
    Serial.println("Playback already in progress...");
    request->send(409, "text/plain", "Playback already in progress");
  }
}

void handleRecordingRequest(AsyncWebServerRequest *request)
{
  Serial.println("Prepare for recording...");

  // extract the record_time from the query string
  String rec_time_str = extractParam(request, "record_time", true);
  if (rec_time_str.isEmpty())
    return;

  record_time = atoi(rec_time_str.c_str());
  if (record_time < 5 || record_time > 30)
  {
    // 415 Unsupported Media Type
    request->send(415, "text/plain", "Cannot record for " + rec_time_str + " seconds");
    return;
  }

  if (recordingTaskHandle == NULL && playbackTaskHandle == NULL)
  {
    // Add code for creating a TASK using the FreeRTOS xTaskCreate API.
    // xTaskCreate(recordingTask, "Record WAV", MIC_I2S_TASK_STACK, NULL, MIC_I2S_TASK_PRIORITY, &recordingTaskHandle);
    xTaskCreatePinnedToCore(recordingTask, "Record WAV", MIC_I2S_TASK_STACK, NULL, MIC_I2S_TASK_PRIORITY, &recordingTaskHandle, 1);
    request->send(200, "text/plain", "Recording started");
  }
  else
  {
    Serial.println("Playback already in progress...");
    request->send(409, "text/plain", "Playback already in progress");
  }
}

void playWavRecording(String path)
{
  File audioFile = FS_TYPE.open(path);
  if (!audioFile)
  {
    Serial.println("Failed to open audio file");
    return;
  }

  // Read and validate WAV header, extract the values from the header
  WAVHeader audioFileHeader;
  if (!fsEnsureWavHeader(audioFile, &audioFileHeader))
  {
    Serial.println("Failed to validate WAV header");
    audioFile.close();
    return;
  }
  Serial.printf("WAV File: Sample Rate: %u, Channels: %u, Bits Per Sample: %u\n", audioFileHeader.sampleRate, audioFileHeader.numChannels, audioFileHeader.bitsPerSample);

  // Init DAC for speakers, using the standard driver
  esp_err_t res = dacInitStd(audioFileHeader.sampleRate, audioFileHeader.bitsPerSample, audioFileHeader.numChannels, DMA_BUF_COUNT, DMA_BUF_LEN, true);
  if (res != ESP_OK)
  {
    Serial.println("Failed to initialize DAC I2S");
    audioFile.close();
    return;
  }
  Serial.println("DAC I2S initialized!");

  // Buffer to hold audio data
  int bufferLen = (int)(BUFF_SIZE / sizeof(char));
  uint8_t buffer[bufferLen];
  size_t bytesRead;
  size_t bytesWritten;

  // Play audio data through I2S
  while (audioFile.available())
  {
    bytesRead = audioFile.read(buffer, sizeof(buffer));
    dacWriteBuff(buffer, bytesRead, &bytesWritten); // audioSTD.h
  }

  // Close file
  audioFile.close();

  // cleanup - uninstall driver
  dacDestroyStd();
}

void playingTask(void *param)
{
  const char *path = (const char *)param;

  if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE)
  {
    // play the file
    digitalWrite(LED, HIGH); // working...
    Serial.println(" *** Play WAV Start *** ");
    playWavRecording(path);
    Serial.println(" *** Play WAV Finished *** ");
    digitalWrite(LED, LOW);     // done...
    xSemaphoreGive(audioMutex); // release semaphore
  }

  // if (playbackTaskHandle != NULL)
  // {
  //   vTaskDelete(playbackTaskHandle);
  //   playbackTaskHandle = NULL;
  // }
  playbackTaskHandle = NULL;
  vTaskDelete(NULL); // delete calling task
}

unsigned long recordWav()
{
  unsigned long flash_wr_size = 0;
  unsigned long flash_record_size = getFlashRecordSize(); // in bytes
  size_t bytes_read;
  size_t bytes_written;
  size_t size_read = sizeof(int32_t);
  size_t size_write = sizeof(int16_t);

  int bufferLen = (int)(BUFF_SIZE / size_read);
  int32_t sBuffer[bufferLen];
  int16_t outputBuffer[bufferLen];
  // int32_t outputBuffer[bufferLen];

  Serial.printf("buffer len for recording %d samples\n", bufferLen);
  Serial.printf("reserved file size: %u\n", flash_record_size);

  while (flash_wr_size < flash_record_size)
  {
    // read data from I2S bus, in this case, from ADC.
    micReadBuff(&sBuffer, bufferLen * size_read, &bytes_read); // audioSTD.h
    if (bytes_read > 0)
    {
      // save original data from I2S(ADC) into flash...

      // Extract 24-bit samples from 32-bit variable and downscale to 16-bit
      for (int i = 0; i < bufferLen; i++)
      {
        int32_t temp = sBuffer[i];
        // Shift right by 11 bits. works for soft sounds, louder sounds may need more reduction
        temp = (temp >> 11);
        // Check the sign bit and extend it if necessary
        if (temp & 0x8000)
        {
          temp |= 0xFFFF0000;
        }

        outputBuffer[i] = (int16_t)temp;
      }
      bytes_written = file_out.write((const byte *)outputBuffer, bufferLen * size_write);
      flash_wr_size += bytes_written;

      if (MONITORING)
      {
        Serial.printf("Recording written %d B\n", bytes_written);
        Serial.printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
      }
    }
    else
    {
      Serial.println("I2S read error");
    }
  }
  return flash_wr_size;
}

// old version
unsigned long _recordWav()
{
  unsigned long flash_wr_size = 0;
  unsigned long flash_record_size = getFlashRecordSize(); // in bytes
  size_t bytes_read;
  size_t bytes_written;
  size_t size_read = sizeof(char);
  size_t size_write = sizeof(char);
  int bufferLen = BUFF_SIZE; // each element is a byte

  char *i2s_read_buff = (char *)calloc(bufferLen, size_read);
  uint8_t *flash_write_buff = (uint8_t *)calloc(bufferLen, size_write);

  // Discard the first two blocks, the microphone may have startup time (i.e. INMP441 up to 83ms)
  Serial.println(" *** Discard the first two blocks *** ");
  micDiscardBlocks((void *)i2s_read_buff, bufferLen * size_read, &bytes_read, 2); // audioSTD.h

  while (flash_wr_size < flash_record_size)
  {
    // read data from I2S bus, in this case, from ADC.
    micReadBuff((void *)i2s_read_buff, bufferLen * size_read, &bytes_read); // audioSTD.h
    if (bytes_read > 0)
    {
      if (MONITORING)
      {
        // printing for debugging
        fsPrintBuffer((uint8_t *)i2s_read_buff, 64); // moved to fsFLASH.h
      }

      // // save original data from I2S(ADC) into flash.
      // // From the buffer, it's ready to write to file.
      // bytes_written = file_out.write((const byte *)i2s_read_buff, bufferLen);

      // Instead of writing directly from the buffer, go through micDataScale and write to the file.
      // This allows to reduce or increase the overall volume including noise.
      micDataScale(flash_write_buff, (uint8_t *)i2s_read_buff, bufferLen);
      bytes_written = file_out.write((const byte *)flash_write_buff, bufferLen * size_write);
      flash_wr_size += bytes_written;

      if (MONITORING)
      {
        Serial.printf("Recording written %d B\n", bytes_written);
        // Serial.printf("Sound recording %u%%\n", flash_wr_size * 100 / flash_record_size);

        // Check the size of the stack in the TASK.
        // uxTaskGetStackHighWaterMark() shows how much Stack was never used. The closer to 0, the better.
        Serial.printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
        // We confirmed that the size of the Stack doesn't lack or make up a large proportion.
      }
    }
    else
    {
      Serial.println("I2S read error");
    }
  }

  // cleanup
  free(i2s_read_buff);
  i2s_read_buff = NULL;

  free(flash_write_buff);
  flash_write_buff = NULL;

  return flash_wr_size;
}

// This i2s_adc_dac is working on a TASK.
// It should be running continuously without return anything.
// If you have a case that you need to turn off, the you have to delete the working TASK by yourself.
// Here, this is a one-time record for now.
// After finishing the recording, the TASK should be done by vTaskDelete(NULL).
void recordingTask(void *param)
{
  if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE)
  {
    // Init microphone for sampling, using the standard driver
    esp_err_t resMic = micInitStd(MIC_SAMPLE_RATE, MIC_SAMPLE_BITS, DMA_BUF_COUNT, DMA_BUF_LEN, true);
    if (resMic != ESP_OK)
    {
      Serial.println("Failed to initialize MIC I2S");
      cleanupRecording(true, false);
      return;
    }
    Serial.println("Microphone I2S initialized!");

    if (!prepareForRecording())
    {
      Serial.println("Failed to create file for recording");
      cleanupRecording(true, false); // file is not available
      return;
    }
    Serial.println("Recording file initialized!");

    // Record audio data + Close file within the task
    digitalWrite(LED, HIGH); // working...
    Serial.println(" *** Recording Start *** ");
    // old verison works with SAMPLE_RATE=16000, SAMPLE_BITS=16, TASK_STACK=4096, BUFF_LEN=64
    unsigned long wavNewSize = 0;
    unsigned long wavSize = getFlashRecordSize();
    if (MIC_SAMPLE_BITS == 16)
    {
      wavNewSize = _recordWav();
    }
    else if (MIC_SAMPLE_BITS == 32)
    {
      wavNewSize = recordWav();
    }

    Serial.println(" *** Recording Finished *** ");

    Serial.printf("actual file size: %u B\n", file_out.size());
    Serial.printf("WAV data size: %u B\n", wavSize);
    Serial.printf("WAV new size: %u B\n", wavNewSize);

    if (wavSize != wavNewSize)
    {
      Serial.printf("Update wav header with new data size: %u B\n", wavNewSize);
      fsUpdateWavHeader(file_out, wavNewSize);
    }

    // Don't forget to close the file after all done.
    file_out.close();
    // cleanup - uninstall driver
    micDestroyStd();

    // re-call listing files
    fsListFiles();

    if (MONITORING)
    {
      // printing for debugging, using the entire file
      Serial.println(" *** Recording WAV content begin *** ");
      fsPrintFileContent(filename_out);
      Serial.println(" *** Recording WAV content end *** ");
    }

    digitalWrite(LED, LOW);     // done...
    xSemaphoreGive(audioMutex); // release semaphore
  }

  cleanupRecording(false, false); // cleanup, no need to release mutex
}

bool prepareForRecording()
{
  // Instead of formatting every time, just removing the previous recording file when it starts.
  fsRemoveFile(filename_out);

  // The "/audio/recording.wav" file starts with this Wave header.
  file_out = FS_TYPE.open(filename_out, FILE_WRITE);
  if (!file_out)
  {
    Serial.println("File is not available!");
    return false;
  }

  // Write WAV header
  // file_out should be open
  byte header[wavHeaderSize];
  // we generate 16-bit header for output file
  fsGenerateWavHeader(header, getFlashRecordSize(), MIC_SAMPLE_RATE, MIC_CHANNEL_NUM, MIC_SAMPLE_BITS_HDR);
  file_out.write(header, wavHeaderSize);

  return true;
}

void cleanupRecording(bool releaseMutex = false, bool closeFile = false)
{
  if (closeFile)
    file_out.close(); // cleanup file
  if (releaseMutex)
    xSemaphoreGive(audioMutex); // release semaphore
  // if (recordingTaskHandle != NULL)
  // {
  //   vTaskDelete(recordingTaskHandle);
  //   recordingTaskHandle = NULL;
  // }
  recordingTaskHandle = NULL;
  vTaskDelete(NULL); // delete calling task
}

String extractParam(AsyncWebServerRequest *request, String param_name, bool post = false)
{
  if (!request->hasParam(param_name, post))
  {
    request->send(400, "text/plain", "Missing param");
    return emptyString;
  }

  String param_value = request->getParam(param_name, post)->value();
  if (param_value.isEmpty())
  {
    request->send(400, "text/plain", "Empty param");
    return emptyString;
  }

  return param_value;
}

// return the path with the audio_dir prefix
String getAudioPath(String filename)
{
  if (filename.startsWith(audio_dir))
    return filename;
  return (audio_dir + filename);
}

// extract filename parameter from the post request
String extractFilePath(AsyncWebServerRequest *request)
{
  String filename = extractParam(request, "file", true);
  if (filename.isEmpty())
    return emptyString;

  String path = getAudioPath(filename);
  if (!fsExists(path))
  {
    request->send(404, "text/plain", "Not found on FS");
    return emptyString;
  }

  return path;
}

// get FLASH_RECORD_SIZE, depends on RECORD_TIME
unsigned long getFlashRecordSize()
{
  return (unsigned long)(MIC_CHANNEL_NUM * MIC_SAMPLE_RATE * MIC_SAMPLE_BITS_HDR / 8 * record_time);
}