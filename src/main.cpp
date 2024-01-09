#include <Arduino.h>

#include <M5Unified.h>
#include <SD.h>
#include <Preferences.h>

#include "lora.h"
#include "audio.h"
#include "usbmidi.h"
#include "ui.h"

Preferences preferences;

void setup(void) {

    Serial.begin(115200);
    Serial.println("Start");

    M5.begin();
    M5.Power.begin();

    // Preferences init
    preferences.begin("HPlayer", false);
    if (preferences.getString("audioout", "") == "")
        preferences.putString("audioout", "SPEAKER");
    
    // DISPLAY init
    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawCenterString("= HPlayer 0 =", 160, 20);


    // LORA init 868MHz
    while (!loraSetup()) M5.Display.drawCenterString("LoRa not found..", 160, 50);
    M5.Display.drawCenterString("    LoRa ok !    ", 160, 50);

    // USB MIDI init
    if (!midiSetup()) M5.Display.drawCenterString("USB not found..", 160, 70);
    else M5.Display.drawCenterString("    USB ok !    ", 160, 70);

    delay(1000);

    // Interface
    // menu_init();

    // AUDIO init
    audioSetup( preferences.getString("audioout") );
    M5.Display.drawCenterString("    Audio: "+audioOUTname(), 160, 90);

    // SD CARD ready ?
    if (audioSDok()) M5.Display.drawCenterString("    SD ok!    ", 160, 110);
    else M5.Display.drawCenterString("SD not found..", 160, 110);

}

void loop(void) 
{   
    loraLoop();
    midiLoop();
    audioLoop();
    // menu_loop();

    while (!loraStackIsEmpty()) {
        byte cmd = loraStackPop();
        M5.Display.drawCenterString( String(cmd), 160, 90);
        
        if (cmd == 255) audioStop();
        else audioPlayKey(cmd);
    }    
    
    

    M5.update();
    
    // if (M5.BtnA.wasClicked()) {
    //     int i = audioNextIndex();
        
    //     loraSend(playingIndex);
    //     audioPlay(filenames[playingIndex]);
    // }

    // if (M5.BtnB.wasClicked()) {
    //     loraSend(255);
    //     audioStop();
    // }

    // if (M5.BtnC.wasClicked()) {
    //     playingIndex = (playingIndex + 1) % file_count;
    //     loraSend(playingIndex);
    //     audioPlay(filenames[playingIndex]);
    // }
}

