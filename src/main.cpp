#include <Arduino.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

#include <M5Unified.h>
#include <M5UnitRCA.h>

AudioFileSourceSD *file;
AudioGeneratorWAV *wav;
AudioOutputI2S *out = NULL;

String filenames[64];
int current_file = 0;
int file_count = 0;

void play_wav(String filepath) 
{
    filepath = "/"+filepath;
    Serial.println("Play "+filepath);
    
    file = new AudioFileSourceSD(filepath.c_str());
    wav  = new AudioGeneratorWAV();
    wav->begin(file, out);

    uint32_t trigAt = 0;

    while(wav->loop());

    Serial.println("WAV Done");

    // Might need to flush DMA buffer?

    wav->stop();

    delete file;
    delete wav;
    wav = NULL;
}

void setup(void) {

    Serial.begin(115200);

    M5.begin();

    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawCenterString("= HPlayer 0 =", 160, 20);
    
    while (!SD.begin(4)) M5.Display.drawCenterString("SD not found", 160, 50);
    M5.Display.drawCenterString("      SD ok !      ", 160, 50);    

    int dma_buffer_count = 8;
    out  = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, dma_buffer_count, AudioOutputI2S::APLL_ENABLE);
    out->SetPinout(13, 0, 15);
    out->SetGain(1.0);
    out->SetChannels(2);

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

void loop(void) {
    M5.update();

    
    if (M5.BtnA.wasClicked()) {
        play_wav(filenames[0]);
    }

    if (M5.BtnB.wasClicked()) {
        play_wav(filenames[1]);
    }
}