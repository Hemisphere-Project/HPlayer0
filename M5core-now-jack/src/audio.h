#include <Arduino.h>

#include <M5Unified.h>
#include <M5UnitRCA.h>

#include <AudioFileSourceSD.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include "class/AudioOutputA2DP.h"

bool sdOK = false;
String audioOutName = "";

AudioFileSourceSD *file;
AudioGeneratorWAV *wav;

AudioOutputI2S *outLINE = NULL;
AudioOutputA2DP *outBT = NULL;
AudioOutput *out = NULL;

const int MAX_FILES = 64;

String fileindexed[MAX_FILES];
String files[MAX_FILES];
int file_count = 0;

byte current_key = 255;
byte current_index = 255;
String current_name = "";



void sdInit()
{
    if (sdOK) return;
    sdOK = SD.begin(4);
    if (!sdOK) return;

    // Create array of filenames from SD card
    file_count = 0;
    File root = SD.open("/");
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) continue;
        String filename = entry.name();
        if (filename.endsWith(".wav")) {

            // Add filename to array
            if (file_count >= MAX_FILES) break;
            files[file_count] = filename;
            file_count++;

            // Add filename to indexed array
            String indexS = "";
            for (int i=0; i<filename.length(); i++) {
                if (!isDigit(filename[i])) break;
                indexS += filename[i];
            }
            if (indexS.length() == 0) continue;
            int index = indexS.toInt();
            if (index >= MAX_FILES) continue;
            fileindexed[index] = filename;
            
            Serial.printf("File %d: %s\n", index, filename.c_str());
        }
        entry.close();
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

void audioSetup(String name)
{
    sdInit();

    if (out != NULL) {
        audioStop();
        delete out;
    }

    audioOutName = name;

    // AUDIO I2S
    if (name == "LINE") {
        int dma_buffer_count = 8;
        outLINE  = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, dma_buffer_count, AudioOutputI2S::APLL_ENABLE);
        outLINE->SetPinout(13, 0, 15);
        outLINE->SetChannels(2);
        outLINE->SetOutputModeMono(false);
        out = outLINE;
    }

    // AUDIO SPEAKER
    else if (name == "SPEAKER") {
        outLINE = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
        outLINE->SetChannels(2);
        outLINE->SetOutputModeMono(true);
        out = outLINE;
    }

    // AUDIO BT
    else {
        outBT = new AudioOutputA2DP(name.c_str());  
        out = outBT;
    }

    out->SetGain(0.8);
}

// 0->127
void audioVolume(int v)
{   
    float f = v / 127.0;
    out->SetGain(f);
}

// PLAY filename
void audioPlay(String filepath = "") 
{   
    audioStop();
    if (out == NULL || !sdOK) return;

    if (filepath.length() == 0) filepath = current_name;
    if (filepath.length() == 0) return;

    current_name = filepath;

    filepath = "/"+filepath;
    Serial.println("Play "+filepath);
    
    file = new AudioFileSourceSD(filepath.c_str());
    wav  = new AudioGeneratorWAV();
    wav->begin(file, out);

    uint32_t trigAt = 0;

}

// PLAY file count
void audioPlayIndex(byte i) 
{   
    if (i == 255) audioStop();
    if (file_count == 0) return;
    i = i % file_count;
    current_index = i;
    current_key = 255;
    audioPlay(files[i]);
}

// PLAY file starting with xx_
void audioPlayKey(byte key) 
{   
    if (key == 255) audioStop();
    if (key < 0 || key >= MAX_FILES) return;
    if (fileindexed[key].length() == 0) return;
    current_key = key;
    current_index = 255;
    audioPlay(fileindexed[key]);
}

void audioLoop()
{
    sdInit();
    if (wav == NULL || wav->loop()) {
        // if (wav) Serial.printf("WAV Loop %d\n", outNULL->GetSamples());
        return;
    }
    Serial.println("WAV Done");
    audioStop();
}

bool audioSDok() {
    return sdOK;
}

String audioOUTname() {
    return audioOutName;
}

byte audioNextKey() {
    byte nextKey;
    for (byte i=1; i<MAX_FILES; i++) {
        nextKey = (current_key+i) % MAX_FILES;
        if (fileindexed[nextKey].length() > 0) return nextKey;
    } 
    return current_key;
}

byte audioPrevKey() {
    byte prevKey;
    for (byte i=1; i<MAX_FILES; i++) {
        prevKey = (current_key-i+MAX_FILES) % MAX_FILES;
        if (fileindexed[prevKey].length() > 0) return prevKey;
    } 
    return current_key;
}

