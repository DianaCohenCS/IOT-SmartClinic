## esp32-audio-recorder Project by: Diana Cohen 
  
## Details about the project
* Web server on ESP32 serving a web page as GUI
* Option to upload audio files (MP3, WAV in different qualities) to local storage (SPIFFS)
* Store at least 3 files at once, and play any file, through the GUI
* Option to start recording from Microphone through GUI – directly to local storage (SPIFFS)
* Ability to play the last recording, through the GUI - "recording.wav" appear on the list of audio files, and can be played/deleted
 
## Folder description
* Documentation: wiring diagram + basic operating instructions.
* ESP32: --deprecated. source code for the esp side (firmware). Unit tests are based on these files. 
* esp32-audio-recorder/src: the most updated .h files, replacing ESP32 content. note that secrets.h is ignored, you need to create your own with credentials.
* esp32-audio-recorder/data: the web server files (html+javascript+css).
* flutter_app: --irrelevant. dart code for Flutter app, but our project has none.
* Parameters: contains description of configurable parameters 
* Unit Tests: tests for individual hardware components (input / output devices).
* Assets: 3D printed parts, Audio files used in this project, different .mp3 and .wav files, which can be uploaded using esp32-audio-recorder and then played in-browser, while the .wav files can play on esp32

## Arduino/ESP32 libraries used in this project:
* XXXX - version XXXXX
* Audio I2S:
  - built-in (~/.arduino15/):
    * driver/i2s (espressif)                              - version 5.1

## Arduino/ESP32 libraries installed for the project:
* XXXX - version XXXXX
* ESP Async WebServer (~/Arduino/libraries):
  - https://github.com/me-no-dev/ESPAsyncWebServer        - version 1.2.4
  - https://github.com/me-no-dev/AsyncTCP                 - version 1.1.1
* Audio I2S:
  - external (~/Arduino/libraries) [not in use]:
    * https://github.com/pschatzmann/arduino-audio-tools  - version 0.9.9
    * https://github.com/pschatzmann/arduino-libhelix     - version 0.8.6
    * https://github.com/schreibfaul1/ESP32-audioI2S      - version 2.0.0

## Project Poster:
 
This project is part of ICST - The Interdisciplinary Center for Smart Technologies, Taub Faculty of Computer Science, Technion
https://icst.cs.technion.ac.il/
