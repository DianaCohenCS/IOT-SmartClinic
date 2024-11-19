// For PlatformIO need to begin with this include
// #include <Arduino.h>

// TODO: need to test the code for both the libraries
#include "fsDEFS.h"

// use SPIFFS by default or LittleFS of your choice
#ifndef USE_LITTLE
#define USE_LITTLE false
#endif

#if USE_LITTLE == false
#include <SPIFFS.h>
#define FS_TYPE SPIFFS
#else
#include <LittleFS.h>
#define FS_TYPE LittleFS
#endif

// You only need to format the filesystem once
#ifndef FS_FORMAT
#define FS_FORMAT false
#endif

// The header of a WAV (RIFF) file is 44 bytes long
const int wavHeaderSize = 44;

struct WAVHeader
{
  uint32_t sampleRate;
  uint16_t numChannels;
  uint16_t bitsPerSample;
};

// init the file system, using the chosen type
void fsInit()
{
  // Implement what SPIFFS needs: init->format->make a file
  if (FS_FORMAT)
  {
    Serial.println("Formatting...");
    FS_TYPE.format();
  }

  if (!FS_TYPE.begin(true))
  {
    Serial.println("Filesystem initialization failed!");
    while (1)
      yield();
  }
}

bool fsRemoveFile(String path)
{
  if (FS_TYPE.exists(path))
    return FS_TYPE.remove(path);
  return true;
}

void fsEnd()
{
  FS_TYPE.end();
}

bool fsExists(String path)
{
  bool res = false;
  File file = FS_TYPE.open(path, "r");
  if (!file.isDirectory())
  {
    res = true; // this is a file
  }
  file.close();
  return res;
}

// format bytes
String fsFormatBytes(size_t bytes)
{
  if (bytes < 1024)
    return String(bytes) + " B";

  if (bytes < (1024 * 1024))
    return String(bytes / 1024.0) + " KB";

  if (bytes < (1024 * 1024 * 1024))
    return String(bytes / 1024.0 / 1024.0) + " MB";

  return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

// get the available space in bytes.
// for formatting, use fsFormatBytes as a wrapper
size_t fsAvailableSpace()
{
  return FS_TYPE.totalBytes() - FS_TYPE.usedBytes();
}

void printSpaces(int spaces)
{
  if (spaces < 1)
    spaces = 1;
  while (spaces--)
    Serial.print(" ");
}

// list the FS files using tabulation
void fsListFiles()
{
  static const char line[] PROGMEM = "=================================================";

  Serial.println(F("\r\nListing files:"));
  Serial.println(FPSTR(line));
  Serial.println(F("File name                                    Size"));
  Serial.println(FPSTR(line));

  fs::File root = FS_TYPE.open("/");
  if (!root)
  {
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println(F("Not a directory"));
    return;
  }

  fs::File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("DIR : ");
      String fileName = file.name();
      Serial.print(fileName);
    }
    else
    {
      String fileName = file.name();
      Serial.print("" + fileName);
      // File path can be 31 characters maximum in SPIFFS
      printSpaces(33 - fileName.length()); // Tabulate nicely

      String fileSize = (String)file.size();
      printSpaces(10 - fileSize.length()); // Tabulate nicely
      Serial.println(fileSize + " bytes");
    }
    file = root.openNextFile();
  }
  Serial.println(FPSTR(line));

  Serial.print("Available FS space: "); // 20 chars
  String fs_space = fsFormatBytes(fsAvailableSpace());
  printSpaces(29 - fs_space.length());
  Serial.println(fs_space);
  Serial.println(FPSTR(line));

  Serial.println();
}

// json ready format
String fsGetPlaylist(String path = "/")
{
  File root = FS_TYPE.open(path);

  String output = "[";
  if (root.isDirectory())
  {
    File file = root.openNextFile();
    while (file)
    {
      // filter only audio files, e.g. ".wav" and ".mp3"
      String fileName = file.name();
      if (fileName.endsWith(".wav") || fileName.endsWith(".mp3"))
      {
        if (output != "[")
        {
          output += ',';
        }
        output += "{\"type\":\"";
        output += (file.isDirectory()) ? "dir" : "file";
        output += "\",\"name\":\"";
        // output += String(file.path()).substring(1);
        // output += "\"}";
        output += fileName;
        output += "\",\"path\":\"";
        output += String(file.path());
        output += "\",\"size\":\"";
        output += fsFormatBytes(file.size());
        output += "\"}";
      }
      file = root.openNextFile();
    }
  }
  output += "]";
  return output;
}

// json ready format
String fsGetSpace()
{
  String output = "{\"space\":\"";
  output += fsFormatBytes(fsAvailableSpace());
  output += "\"}";
  return output;
}

// A header is required to complete the Wave file.
// A WAVE file is often just a RIFF file with a single "WAVE" chunk which consists of two sub-chunks:
// a "fmt" chunk specifying the data format and a "data" chunk containing the actual sample data.
void fsGenerateWavHeader(byte *header, unsigned long wavSize, int sampleRate = 16000, int numChannels = 1, int bitsPerSample = 16)
{
  // header[0-3] ChunkID: "RIFF" Marks the file as a riff file. Characters are each 1 byte long.
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';

  // header[4-7] ChunkSize: Size of the overall file - 8 bytes, in bytes (32-bit integer).
  // Typically, you’d fill this in after creation.
  unsigned long fileSize = wavSize + wavHeaderSize - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);

  // header[8-11] Format: "WAVE" File Type Header. For our purpose, it always equals "WAVE".
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';

  // header[12-15] Subchunk1ID: "fmt", Format chunk marker. Includes trailing null.
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';

  // header[16-19] Subchunk1Size: 16, Length of format data as listed above
  header[16] = 0x10;
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;

  // header[20-21] AudioFormat: 1, Type of format (1 is PCM) - 2 byte integer
  header[20] = 0x01;
  header[21] = 0x00;

  // header[22-23] NumChannels: 1 (or 2), Number of Channels - 2 byte integer
  header[22] = (byte)numChannels; // 0x01;
  header[23] = 0x00;

  // header[24-27] SampleRate: 16000=0x3E80, Sample Rate - 32 byte integer
  // I think this is 32-bit integer, not byte
  // Common values are 44100=0xAC44 (CD), 48000=0xBB80 (DAT).
  // Sample Rate = Number of Samples per second, or Hertz.
  header[24] = (byte)(sampleRate & 0xFF);
  header[25] = (byte)((sampleRate >> 8) & 0xFF);
  header[26] = (byte)((sampleRate >> 16) & 0xFF);
  header[27] = (byte)((sampleRate >> 24) & 0xFF);

  // header[28-31] ByteRate: 32000=0x7D00, (SampleRate * BitsPerSample * Channels) / 8
  // in our case, (16000 * 16 * 1) / 8 = 32000
  // another useful scenario: (44100 * 16 * 2) / 8 = 176400 = 0x2B110
  unsigned long byteRate = (sampleRate * bitsPerSample * numChannels) / 8;
  header[28] = (byte)(byteRate & 0xFF);
  header[29] = (byte)((byteRate >> 8) & 0xFF);
  header[30] = (byte)((byteRate >> 16) & 0xFF);
  header[31] = (byte)((byteRate >> 24) & 0xFF);

  // header[32-33] BlockAlign: 2, (BitsPerSample * Channels) / 8
  // 1 - 8 bit mono, 2 - 8 bit stereo/16 bit mono, 4 - 16 bit stereo
  unsigned int blockAlign = (bitsPerSample * numChannels) / 8;
  header[32] = (byte)blockAlign;
  header[33] = 0x00;

  // header[34-35] BitsPerSample: 16=0x10, 32=0x20, Bits per sample
  header[34] = (byte)bitsPerSample;
  header[35] = 0x00;

  // header[36-39] Subchunk2ID: "data", Data chunk header. Marks the beginning of the data section.
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';

  // header[40-43] Subchunk2Size: File size (data), Size of the data section.
  header[40] = (byte)(wavSize & 0xFF);
  header[41] = (byte)((wavSize >> 8) & 0xFF);
  header[42] = (byte)((wavSize >> 16) & 0xFF);
  header[43] = (byte)((wavSize >> 24) & 0xFF);
}

void fsUpdateWavHeader(File file, unsigned long dataSize)
{
  file.seek(4);
  unsigned long riffSize = dataSize + wavHeaderSize - 8;
  file.write((byte)(riffSize & 0xFF));
  file.write((byte)((riffSize >> 8) & 0xFF));
  file.write((byte)((riffSize >> 16) & 0xFF));
  file.write((byte)((riffSize >> 24) & 0xFF));

  file.seek(40);
  file.write((byte)(dataSize & 0xFF));
  file.write((byte)((dataSize >> 8) & 0xFF));
  file.write((byte)((dataSize >> 16) & 0xFF));
  file.write((byte)((dataSize >> 24) & 0xFF));
}

// bool fsEnsureWavHeader(File file) // deprecated
bool fsEnsureWavHeader(File file, WAVHeader *wavHeader = NULL)
{
  char header[wavHeaderSize];
  if (file.read((uint8_t *)header, wavHeaderSize) != wavHeaderSize)
  {
    Serial.println("Invalid WAV file");
    return false;
  }

  // Check "RIFF" and "WAVE" chunks
  if (strncmp(header, "RIFF", 4) != 0 || strncmp(header + 8, "WAVE", 4) != 0)
  {
    Serial.println("Invalid WAV file");
    return false;
  }

  // Check audio format (should be PCM)
  int audioFormat = header[20] | (header[21] << 8);
  if (audioFormat != 1)
  {
    Serial.println("Unsupported WAV format");
    return false;
  }

  // Check number of channels (should be 1 or 2)
  int numChannels = header[22] | (header[23] << 8);
  if (numChannels != 1 && numChannels != 2)
  {
    Serial.println("Unsupported number of channels");
    return false;
  }

  // Check sample rate
  int sampleRate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
  if (sampleRate < 8000 || sampleRate > 192000)
  {
    Serial.println("Unsupported sample rate");
    return false;
  }

  // Check bits per sample
  int bitsPerSample = header[34] | (header[35] << 8);
  if (bitsPerSample != 16 && bitsPerSample != 32 && bitsPerSample != 24 && bitsPerSample != 8)
  {
    Serial.println("Unsupported bits per sample");
    return false;
  }

  // Extract relevant information - get the pointer to a struct
  if (wavHeader != NULL)
  {
    wavHeader->sampleRate = (uint32_t)sampleRate;
    wavHeader->numChannels = (uint16_t)numChannels;
    wavHeader->bitsPerSample = (uint16_t)bitsPerSample;
  }
  return true;
}

// For the debugging reason, let's print out the values on the buffer.
void fsPrintBuffer(uint8_t *buf, int length)
{
  printf("======\n");
  for (int i = 0; i < length; i++)
  {
    printf("%02x ", buf[i]);
    if ((i + 1) % 8 == 0)
    {
      printf("\n");
    }
  }
  printf("======\n");
}

// Print Average reading to serial plotter
void fsPrintSamplesAVG(uint8_t *buf, int length)
{
  // Read I2S data buffer
  int samples_read = length / 8;
  if (samples_read > 0)
  {
    float mean = 0;
    for (int i = 0; i < samples_read; ++i)
    {
      mean += buf[i];
    }

    // Average the data reading
    mean /= samples_read;

    // Print to serial plotter
    Serial.println(mean);
  }
}

void fsPrintFileContent(String path)
{
  File file = FS_TYPE.open(path, "r");
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Read WAV header
  if (!fsEnsureWavHeader(file))
  {
    Serial.println("Failed to read WAV header");
    file.close();
    return;
  }

  // Buffer to hold audio data
  uint8_t buffer[64];
  size_t bytes_read;

  Serial.println("File Content:");
  while (file.available())
  {
    // Serial.write(file.read());
    bytes_read = file.read(buffer, sizeof(buffer));
    // fsPrintBuffer((uint8_t*)buffer, sizeof(buffer));
    fsPrintSamplesAVG((uint8_t *)buffer, sizeof(buffer));
  }
  Serial.println();

  file.close();
}
