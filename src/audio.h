#include <Arduino.h>

#include <M5Unified.h>
#include <M5UnitRCA.h>

#include <AudioFileSourceSD.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include "class/AudioOutputA2DP.h"

AudioFileSourceSD *file;
AudioGeneratorWAV *wav;

AudioOutputI2S *outLINE = NULL;
AudioOutputA2DP *outBT = NULL;
AudioOutput *out = NULL;

String filenames[64];
int current_file = 0;
int file_count = 0;


void audioSetup(const char* btSSID = NULL)
{
    // AUDIO I2S
    if (btSSID == NULL) {
        int dma_buffer_count = 8;
        outLINE  = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, dma_buffer_count, AudioOutputI2S::APLL_ENABLE);
        outLINE->SetPinout(13, 0, 15);
        outLINE->SetChannels(2);
        out = outLINE;
    }

    // AUDIO BT
    else {
        outBT = new AudioOutputA2DP(btSSID);  
        out = outBT;
    }

    out->SetGain(0.7);

    // Create array of filenames from SD card
    file_count = 0;
    File root = SD.open("/");
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) continue;
        String filename = entry.name();
        if (filename.endsWith(".wav")) {
            filenames[file_count] = filename;
            file_count++;
        }
        entry.close();
        Serial.println(filename);
    }
    root.close();    
}

void audioStop()
{
    if (wav == NULL) return;
    wav->stop();
    delete file;
    delete wav;
    wav = NULL;
}

void audioPlay(String filepath) 
{   
    audioStop();

    filepath = "/"+filepath;
    Serial.println("Play "+filepath);
    
    file = new AudioFileSourceSD(filepath.c_str());
    wav  = new AudioGeneratorWAV();
    wav->begin(file, out);

    uint32_t trigAt = 0;
    }

void audioLoop()
{
    if (wav == NULL || wav->loop()) {
        // if (wav) Serial.printf("WAV Loop %d\n", outNULL->GetSamples());
        return;
    }
    Serial.println("WAV Done");
    audioStop();
}

