#include <Arduino.h>
#include <M5Unified.h>
#include "lora.h"
#include "audio.h"


void setup(void) {

    Serial.begin(115200);

    M5.begin();
    M5.Power.begin();

    // DISPLAY init
    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawCenterString("= HPlayer 0 =", 160, 20);
    
    // SD CARD init
    while (!SD.begin(4)) M5.Display.drawCenterString("SD not found..", 160, 50);
    M5.Display.drawCenterString("      SD ok !      ", 160, 50);    

    // LORA init 868MHz
    LoRa.setPins(); 
    while (!LoRa.begin(868E6))  M5.Display.drawCenterString("LoRa not found..", 160, 70);
    M5.Display.drawCenterString("    LoRa ok !    ", 160, 70);

    // AUDIO init
    audioSetup();

}

void loop(void) 
{
    loraLoop();
    audioLoop();

    M5.update();
    
    if (M5.BtnA.wasClicked()) {
        loraSend("play "+filenames[0]);
        audioPlay(filenames[0]);
    }

    if (M5.BtnB.wasClicked()) {
        loraSend("play "+filenames[1]);
        audioPlay(filenames[1]);
    }

    if (M5.BtnC.wasClicked()) {
        loraSend("stop");
        audioStop();
    }
}