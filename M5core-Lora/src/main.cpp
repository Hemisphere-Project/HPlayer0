#include <Arduino.h>

#include <M5Unified.h>
#include <SD.h>
#include <Preferences.h>

#include "audio.h"
#include <usbmidi.h>
#include <lora.h>
#include <serialcmd.h>

Preferences preferences;

bool midiDetected = false;
bool sdDetected = false;

// #define AUDIO_OUT "BM2"  // LINE or SPEAKER or BT ssid (Now will be disabled if using BT -> plug an I2C side-kick)

/////////////////////////////////////////////////////

void sendCmd(byte cmd) 
{
    loraSend(cmd);
    serialcmdSend(cmd);
}

/////////////////////////////////////////////////////

void setup(void) {

    Serial.begin(115200);
    Serial.println("Start");

    M5.begin();
    M5.Power.begin();

    // Preferences init
    preferences.begin("HPlayer", false);
    if (preferences.getString("audioout", "") == "")
        preferences.putString("audioout", "LINE");
    
    #ifdef AUDIO_OUT
        if (preferences.getString("audioout", "") != AUDIO_OUT)
            preferences.putString("audioout", AUDIO_OUT);
    #endif

    String audioOUT = preferences.getString("audioout");
    
    // DISPLAY init
    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("= HPlayer 0 =", 10, 20);

    // USB MIDI init
    // M5.Display.drawString("USB", 10, 50);
    // M5.Display.drawString(": not found.." , 80, 50);
    midiSetup();

    // SD CARD ready ?
    M5.Display.drawString("SD", 10, 70);
    M5.Display.drawString(": not found.." , 80, 70);

    // AUDIO init
    audioSetup( audioOUT );
    M5.Display.drawString("Audio", 10, 90);
    M5.Display.drawString(": "+audioOUTname(), 80, 90);

    audioVolume(20);

    // SERIALCMD init
    M5.Display.drawString("Serial", 10, 110);
    if (!serialcmdSetup( 22, 21 )) M5.Display.drawString(": not found..", 80, 110); 
    else M5.Display.drawString(": ok         ", 80, 110);

    // LORA init
    M5.Display.drawString("LoRa", 10, 130);
    if (!loraSetup()) M5.Display.drawString(": not found..", 80, 130); 
    else M5.Display.drawString(": ok         ", 80, 130);



    // Interface
    // delay(3000);
    // menu_init();

}

/////////////////////////////////////////////////////

void loop(void) 
{   
    // MIDI hot plug
    if (!midiDetected && midiOK()) {
        midiDetected = true;
        M5.Display.drawString("USB", 10, 50);
        M5.Display.drawString(": ok            ", 80, 50);
    }

    // SD hot plug
    if (!sdDetected && audioSDok()) {
        sdDetected = true;
        M5.Display.drawString(": ok            ", 80, 70);
    }

    midiLoop();
    audioLoop();
    loraLoop();
    serialcmdLoop();
    // menu_loop();

    while (!loraStackIsEmpty()) {
        byte cmd = loraStackPop();
        M5.Display.drawString( "LORA IN: "+String(cmd)+"         ", 10, 170);
        serialcmdSend(cmd); // forward received LoRa to serialcmd
        if (cmd == 255) audioStop();
        else audioPlayKey(cmd);
    }

    while (!serialcmdStackIsEmpty()) {
        byte cmd = serialcmdStackPop();
        M5.Display.drawString( "SERIAL IN: "+String(cmd)+"         ", 10, 170);
        if (cmd == 255) audioStop();
        else audioPlayKey(cmd);
    }

    while (!midiStackIsEmpty()) {
        byte cmd = midiStackPop();
        M5.Display.drawString( "MIDI IN: "+String(cmd)+"         ", 10, 170);
        sendCmd(cmd);
        if (cmd == 255) audioStop();
        else audioPlayKey(cmd);
    } 

    M5.update();
    
    if (M5.BtnA.wasClicked()) {
        byte i = audioPrevKey();
        M5.Display.drawString( "PREV: "+String(i)+"         ", 10, 170);
        sendCmd(i);
        audioPlayKey(i);
    }

    if (M5.BtnB.wasClicked()) {
        M5.Display.drawString( "STOP                  ", 10, 170);
        sendCmd(255);
        audioStop();
    }

    if (M5.BtnC.wasClicked()) {
        byte i = audioNextKey();
        M5.Display.drawString( "NEXT: "+String(i)+"         ", 10, 170);
        sendCmd(i);
        audioPlayKey(i);
    }

}
