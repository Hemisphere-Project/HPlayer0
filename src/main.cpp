#include <Arduino.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

#include <M5Unified.h>
#include <M5UnitRCA.h>

AudioFileSourceSD *file;
AudioGeneratorWAV *wav;
AudioOutputI2S *out = NULL;

void i2sWatcherTask(void *param)
{
    AudioOutputI2S *output = (AudioOutputI2S *)param;

    while(!output || !output->i2sQueue) delay(100);

    while (true)
    {
        if (!wav || !wav->isRunning()) {
            delay(1);
            continue;
        }

        i2s_event_t evt;
        if (xQueueReceive(output->i2sQueue, &evt, portMAX_DELAY) == pdPASS);
        {
            if (evt.type == I2S_EVENT_TX_DONE)
            {
                output->tx_count++;
                // Serial.println("I2S_EVENT_TX_DONE");
                
                if (output->tx_count == 12) {
                    digitalWrite(21, HIGH);
                    digitalWrite(22, HIGH);
                }
            }
        }
    }
}

void play_wav(String filepath) 
{
    filepath = "/"+filepath+".wav";
    
    Serial.println("Play "+filepath);
    
    file = new AudioFileSourceSD(filepath.c_str());

    out->RegisterSamplesCB([](int sampleCount) {
        // if (sampleCount == 990) {
        //     digitalWrite(21, HIGH);
        //     digitalWrite(22, HIGH);
        // }
    });
    
    wav  = new AudioGeneratorWAV();
    wav->begin(file, out);

    uint32_t trigAt = 0;

    while(wav->loop());

    digitalWrite(21, LOW);
    digitalWrite(22, LOW);

    Serial.println("WAV Done");

    // Might need to flush DMA buffer?

    wav->stop();

    delete file;
    delete wav;
}

void setup(void) {

    Serial.begin(115200);

    M5.begin();

    pinMode(21, OUTPUT);
    pinMode(22, OUTPUT);
    digitalWrite(21, LOW);
    digitalWrite(22, LOW);

    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawCenterString("= HPlayer 0 =", 160, 20);

    if (!SD.begin(4)) M5.Display.drawCenterString("SD not found", 160, 50);
    else M5.Display.drawCenterString("SD ok !", 160, 50);    

    int dma_buffer_count = 8;
    out  = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, dma_buffer_count, AudioOutputI2S::APLL_ENABLE);
    out->SetPinout(13, 0, 15);
    out->SetGain(1.0);
    out->SetChannels(2);

    xTaskCreate(i2sWatcherTask, "i2s Watcher Task", 4096, out, 5, NULL);
}

void loop(void) {
    M5.update();

    if (M5.BtnA.wasClicked()) {
        play_wav("sine440-100ms-stereo");
    }

    if (M5.BtnB.wasClicked()) {
        play_wav("0_marimba");
    }

    
}