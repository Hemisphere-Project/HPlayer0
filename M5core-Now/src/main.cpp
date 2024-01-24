#include <Arduino.h>

#include <M5Unified.h>
#include <SD.h>
#include <Preferences.h>

#include "audio.h"
#include <usbmidi.h>
#include <nowcmd.h>
#include <serialcmd.h>

Preferences preferences;

bool midiDetected = false;
bool sdDetected = false;

//#define AUDIO_OUT "BM2"  // LINE or SPEAKER or BT ssid (Now will be disabled if using BT -> plug an I2C side-kick)

/////////////////////////////////////////////////////

void sendCmd(byte cmd) 
{
    nowBroadcast(cmd);
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
    M5.Display.drawString("USB", 10, 50);
    M5.Display.drawString(": not found.." , 80, 50);
    midiSetup();

    // SD CARD ready ?
    M5.Display.drawString("SD", 10, 90);
    M5.Display.drawString(": not found.." , 80, 90);

    // AUDIO init
    audioSetup( audioOUT );
    M5.Display.drawString("Audio", 10, 110);
    M5.Display.drawString(": "+audioOUTname(), 80, 110);

    audioVolume(10);

    // ESP Link (if not BT in use)
    if (!audioIsBT()) {
        M5.Display.drawString("NLink", 10, 130);
        if (!nowSetup()) M5.Display.drawString(": not ready..", 80, 130); 
        else M5.Display.drawString(": ok         ", 80, 130);

        // Serial Link Disabled
        M5.Display.drawString("Serial", 10, 70);
        M5.Display.drawString(": disabled   ", 80, 70);
    }
    else {
        M5.Display.drawString("NLink", 10, 130);
        M5.Display.drawString(": disabled   ", 80, 130);

        // Serial Link Enabled
        M5.Display.drawString("Serial", 10, 70);
        if (!serialcmdSetup(22, 21)) M5.Display.drawString(": not ready..", 80, 70); 
        else M5.Display.drawString(": ok         ", 80, 70);
    }

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
        M5.Display.drawString(": ok            ", 80, 90);
    }

    midiLoop();
    audioLoop();
    nowLoop();
    serialcmdLoop();
    // menu_loop();

    while (!nowStackIsEmpty()) {
        byte cmd = nowStackPop();
        M5.Display.drawString( "NOW IN: "+String(cmd)+"         ", 10, 170);
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
        M5.Display.drawString( "STOP         ", 10, 170);
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

