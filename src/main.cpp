#include <Arduino.h>

#include <M5Unified.h>
#include <SD.h>
#include <Preferences.h>

#include "lora.h"
#include "audio.h"
#include "usbmidi.h"
#include "ui.h"

Preferences preferences;

bool midiDetected = false;
bool sdDetected = false;

void setup(void) {

    Serial.begin(115200);
    Serial.println("Start");

    M5.begin();
    M5.Power.begin();

    // Preferences init
    preferences.begin("HPlayer", false);
    if (preferences.getString("audioout", "") == "")
        preferences.putString("audioout", "LINE");
    
    // DISPLAY init
    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("= HPlayer 0 =", 10, 20);

    // USB MIDI init
    midiSetup();
    M5.Display.drawString("USB", 10, 50);
    M5.Display.drawString(": not found.." , 80, 50);

    // LORA init 868MHz
    M5.Display.drawString("LoRa", 10, 70);
    if (!loraSetup()) M5.Display.drawString(": not found..", 80, 70);
    else M5.Display.drawString(": ok        ", 80, 70);

    // SD CARD ready ?
    M5.Display.drawString("SD", 10, 90);
    M5.Display.drawString(": not found.." , 80, 90);

    // AUDIO init
    audioSetup( preferences.getString("audioout") );
    M5.Display.drawString("Audio", 10, 110);
    M5.Display.drawString(": "+audioOUTname(), 80, 110);


    // Interface
    // delay(3000);
    // menu_init();

}

void loop(void) 
{   
    // MIDI hot plug
    if (!midiDetected && midiOK()) {
        midiDetected = true;
        M5.Display.drawString(": ok            ", 80, 50);
    }

    // SD hot plug
    if (!sdDetected && audioSDok()) {
        sdDetected = true;
        M5.Display.drawString(": ok            ", 80, 90);
    }

    loraLoop();
    midiLoop();
    audioLoop();
    // menu_loop();

    while (!loraStackIsEmpty()) {
        byte cmd = loraStackPop();
        M5.Display.drawString( "Lora IN: "+String(cmd)+"    ", 10, 170);
        audioPlayKey(cmd);
    }

    while (!midiStackIsEmpty()) {
        byte cmd = midiStackPop();
        loraSend(cmd);
        audioPlayKey(cmd);
    } 


    M5.update();
    
    if (M5.BtnA.wasClicked()) {
        byte i = audioPrevKey();
        loraSend(i);
        audioPlayKey(i);
    }

    if (M5.BtnB.wasClicked()) {
        loraSend(255);
        audioStop();
    }

    if (M5.BtnC.wasClicked()) {
        byte i = audioNextKey();
        loraSend(i);
        audioPlayKey(i);
    }
}

