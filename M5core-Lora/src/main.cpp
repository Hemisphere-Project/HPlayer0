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

// #define AUDIO_OUT "LINE"  // LINE or SPEAKER or BT ssid (Now will be disabled if using BT -> plug an I2C side-kick)
// #define USBMIDI 0           // 0: off, 1: on

/////////////////////////////////////////////////////

void sendCmd(byte cmd) 
{
#if M5CORE    
    loraSend(cmd);
#endif
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

    if (preferences.getInt("usbmidi", 255) == 255)
        preferences.putInt("usbmidi", 0);
    
    #ifdef AUDIO_OUT
        if (preferences.getString("audioout", "") != AUDIO_OUT)
            preferences.putString("audioout", AUDIO_OUT);
    #endif

    #ifdef USBMIDI
        if (preferences.getInt("usbmidi", 255) != USBMIDI)
            preferences.putInt("usbmidi", USBMIDI);
    #endif

    String audioOUT = preferences.getString("audioout");
    int useMIDI = preferences.getInt("usbmidi"); 

    // AUDIO init
    audioSetup( audioOUT );
    audioVolume(20);
    
#if M5CORE 
    // DISPLAY init
    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("= HPlayer 0 =", 10, 20);

    // USB MIDI init
    if (useMIDI) {
        M5.Display.drawString("MIDI", 10, 50);
        M5.Display.drawString(": not found.." , 80, 50);
        midiSetup();
    }
    // SERIALCMD init
    else {    
        M5.Display.drawString("Serial", 10, 50);
        if (!serialcmdSetup( 22, 21 )) M5.Display.drawString(": not found..", 80, 50); 
        else M5.Display.drawString(": ok         ", 80, 50);
    }

    // SD CARD ready ?
    M5.Display.drawString("SD", 10, 70);
    M5.Display.drawString(": not found.." , 80, 70);

    // LORA init
    M5.Display.drawString("LoRa", 10, 130);
    if (!loraSetup()) M5.Display.drawString(": not found..", 80, 130); 
    else M5.Display.drawString(": ok         ", 80, 130);

    // AUDIO OUT
    M5.Display.drawString("Audio", 10, 90);
    M5.Display.drawString(": "+audioOUTname(), 80, 90);

#elif M5ATOM
    serialcmdSetup( 26, 32 ); 
#endif


    // Interface
    // delay(3000);
    // menu_init();

}

/////////////////////////////////////////////////////

void loop(void) 
{   

    // SD hot plug
    //
    if (!sdDetected && audioSDok()) {
        sdDetected = true;
#if M5CORE
        M5.Display.drawString(": ok            ", 80, 70);
#endif
        Serial.println("SD OK");
    }

#if M5CORE
    // MIDI hot plug
    //
    if (!midiDetected && midiOK()) {
        midiDetected = true;
        M5.Display.drawString("MIDI", 10, 50);
        M5.Display.drawString(": ok            ", 80, 50);
    }

    midiLoop();
    loraLoop();
    // menu_loop();
#endif
    serialcmdLoop();
    audioLoop();

#if M5CORE
    // LORA RCV
    //
    while (!loraStackIsEmpty()) {
        byte cmd = loraStackPop();
        M5.Display.drawString( "LORA IN: "+String(cmd)+"         ", 10, 170);
        
        serialcmdSend(cmd); // forward received LoRa to serialcmd
        
        // Volume
        if (cmd >= 100 && cmd <= 227) {
            Serial.println("Volume: "+String(cmd-100));
            audioVolume(cmd-100);
        }
        // Media
        else if (cmd == 255) audioStop();
        else if (cmd < 100) audioPlayKey(cmd);
    }

    // MIDI RCV
    //
    while (!midiStackIsEmpty()) {
        byte* data = midiStackPop();
        byte dest = data[0];
        byte cmd = data[1];
        M5.Display.drawString( "MIDI IN: "+String(cmd)+"         ", 10, 170);
        sendCmd(cmd);
        if (cmd == 255) audioStop();
        else audioPlayKey(cmd);
    }
#endif 

    // SERIAL1 RCV
    //
    while (!serialcmdStackIsEmpty()) {
        byte cmd = serialcmdStackPop();
        #if M5CORE
            M5.Display.drawString( "SERIAL IN: "+String(cmd)+"         ", 10, 170);
        #endif
            Serial.println("SERIAL IN: "+String(cmd));

        // Volume
        if (cmd >= 100 && cmd <= 227) {
            Serial.println("Volume: "+String(cmd-100));
            audioVolume(cmd-100);
        }
        // Media
        else if (cmd == 255) audioStop();
        else if (cmd < 100) audioPlayKey(cmd);
    }

    M5.update();

#if M5CORE
    if (M5.BtnA.wasClicked()) {
        byte i = audioPrevKey();
        M5.Display.drawString( "PREV: "+String(i)+"         ", 10, 170);
        // sendCmd(i);
        serialcmdSend(i);
        audioPlayKey(i);
    }

    if (M5.BtnB.wasClicked()) {
        M5.Display.drawString( "STOP                  ", 10, 170);
        // sendCmd(255);
        serialcmdSend(255);
        audioStop();
    }

    if (M5.BtnC.wasClicked()) {
        byte i = audioNextKey();
        M5.Display.drawString( "NEXT: "+String(i)+"         ", 10, 170);
        // sendCmd(i);
        serialcmdSend(i);
        audioPlayKey(i);
    }
#elif M5ATOM
    if (M5.BtnA.wasPressed()) {
        byte i = audioNextKey();
        sendCmd(i);
        audioPlayKey(i);
    }
#endif

}
