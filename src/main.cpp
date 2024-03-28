#include <Arduino.h>

#include <M5Unified.h>
#include <SD.h>
#include <Preferences.h>

#include "audio.h"
#include <usbmidi.h>
#include <lora.h>
#include <serialcmd.h>

#ifdef M5ATOM
    #include <FastLED.h>
    CRGB mainLED;
#endif

Preferences preferences;

// Default values
byte destID     = 255;
String audioOUT = "SPEAKER";
int useMIDI     = 0;

// Uncomment to burn to flash !!
// #define DEST_ID     3            // DEST_ID: 0=regie 255=all
// #define AUDIO_OUT   "LINE"        // LINE or SPEAKER or BTssid
// #define USBMIDI     0            // 0: off, 1: on

/////////////////////////////////////////////////////
// DISPLAY
/////////////////////////////////////////////////////

void displayStatus(String name, bool status, int y, String txt = "") {
    #if M5CORE
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString(name, 10, y);
    if (!status) {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.drawString( (txt=="") ? "not found.." : txt, 90, y); 
    }
    else {
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.drawString( (txt=="") ? "ok              " : txt, 90, y);
    }
    #endif
}


/////////////////////////////////////////////////////
// CMD PROCESSOR / SENDER
////////////////////////////////////////////////////

void sendCmd(byte dest, byte cmd) 
{
    if ( loraSend(dest, cmd) ) {
        String d = (dest==255)?"":"<"+String(dest)+">";
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.drawString( "Lora OUT "+d+": "+String(cmd)+"             ", 10, 190);
    }

    serialcmdSend(dest, cmd);
}

bool processCmd(byte dest, byte cmd) {
    if (dest == 255 || dest == destID) {
        // Volume
        if (cmd >= 100 && cmd <= 227) {
            Serial.println("Volume: "+String(cmd-100));
            audioVolume(cmd-100);
        }
        // Media
        else if (cmd == 255) audioStop();
        else if (cmd < 100) audioPlayKey(cmd);
        return true;
    }
    return false;
}


/////////////////////////////////////////////////////
// HOTPLUG DETECT
/////////////////////////////////////////////////////
bool midiDetected = false;
bool sdDetected = false;
bool audioConnected = false;

void hotplug() {
    // SD hot plug
    //
    if (sdDetected != audioSDok()) {
        sdDetected = audioSDok();
        displayStatus("SD", sdDetected, 70);
        if (sdDetected) Serial.println("SD OK");
        else Serial.println("SD not found..");
    }

    // MIDI hot plug
    //
    if (useMIDI && midiDetected != midiOK()) {
        midiDetected = midiOK();
        displayStatus("MIDI", midiDetected, 90);
    }

    // BT hot plug
    //
    if (audioConnected != audioLINKok()) {
        audioConnected = audioLINKok();
        displayStatus("Audio", audioConnected, 50, audioOUTname());

        #ifdef M5ATOM
            // GREEN Led ATOM 
            mainLED = audioConnected ? CRGB::Green : CRGB::Red;
            FastLED.show();
        #endif  
    }
}


/////////////////////////////////////////////////////

void setup(void) {

    Serial.begin(115200);
    Serial.println("Start");

    M5.begin();
    M5.Power.begin();

    // Preferences init
    preferences.begin("HPlayer0", false);

    #ifdef AUDIO_OUT
        if (preferences.getString("audioout", audioOUT) != AUDIO_OUT)
            preferences.putString("audioout", AUDIO_OUT);
    #endif

    #ifdef USBMIDI
        if (preferences.getInt("usbmidi", useMIDI) != USBMIDI)
            preferences.putInt("usbmidi", USBMIDI);
    #endif

    #ifdef DEST_ID
        if (preferences.getInt("destid", destID) != DEST_ID)
            preferences.putInt("destid", DEST_ID);
    #endif

    destID      = preferences.getInt("destid",      destID);
    useMIDI     = preferences.getInt("usbmidi",     useMIDI); 
    audioOUT    = preferences.getString("audioout", audioOUT);

    // AUDIO init
    audioSetup( audioOUT );
    audioVolume(20);
    
#if M5CORE 
    // DISPLAY init
    M5.Display.clear(TFT_BLACK);
    M5.Display.setFont(&DejaVu18);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.drawString(".::HPlayer0::.", 10, 20);

    // Dest ID
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.drawString( "#"+String(destID), 260, 20);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    // AUDIO OUT
    displayStatus("Audio", false, 50, audioOUTname());

    // SD CARD ready ?
    displayStatus("SD", false, 70);

    // USB MIDI init
    if (useMIDI) {
        displayStatus("MIDI", false, 90);

        midiSetup([](byte dest, byte cmd) {
            sendCmd(dest, cmd); // forward received MIDI => LoRa & Serial1
            processCmd(dest, cmd);
            String d = (dest==255)?"":"<"+String(dest)+">";
            M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
            M5.Display.drawString( "Midi IN "+d+": "+String(cmd)+"         ", 10, 170);
        });
    }

    // SERIALCMD init
    else {    
        bool serialstatus = serialcmdSetup( 22, 21, [](byte dest, byte cmd) {
            String d = (dest==255)?"":"<"+String(dest)+">";
            if ( processCmd(dest, cmd) ) {
                M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
                M5.Display.drawString( "Serial IN "+d+": "+String(cmd)+"         ", 10, 170);
            }
            Serial.println("Serial IN "+d+": "+String(cmd));
        });
        displayStatus("Serial1", serialstatus, 90);
    }

    // LORA init
    bool lorastatus = loraSetup( [](byte dest, byte cmd) {
            serialcmdSend(dest, cmd); // forward received LoRa => Serial1
            if ( processCmd(dest, cmd) ) {
                M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
                M5.Display.drawString( "LORA IN: "+String(cmd)+"         ", 10, 170);
            }
        });
    displayStatus("LoRa", lorastatus, 110);

#elif M5ATOM
    serialcmdSetup( 26, 32, [](byte dest, byte cmd) {
        processCmd(dest, cmd);
        if (dest = 255 || dest == destID) 
            Serial.println("SERIAL IN: "+String(cmd));
    }); 

    // RED Led ATOM 
    FastLED.addLeds<NEOPIXEL, 27>(&mainLED, 1);
    FastLED.setBrightness(37);
    mainLED = CRGB::Red;
    FastLED.show();   
#endif

}

/////////////////////////////////////////////////////

void loop(void) 
{   
    hotplug();
    midiLoop();
    loraLoop();
    serialcmdLoop();
    audioLoop();
    M5.update();


#if M5CORE
    if (M5.BtnA.wasClicked()) {
        byte i = audioPrevKey();
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.drawString( "PREV: "+String(i)+"         ", 10, 170);
        // sendCmd(i);
        serialcmdSend(255, i);
        audioPlayKey(i);
    }

    if (M5.BtnB.wasClicked()) {
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.drawString( "STOP                  ", 10, 170);
        // sendCmd(255);
        serialcmdSend(255, 255);
        audioStop();
    }

    if (M5.BtnC.wasClicked()) {
        byte i = audioNextKey();
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.drawString( "NEXT: "+String(i)+"         ", 10, 170);
        // sendCmd(i);
        serialcmdSend(255, i);
        audioPlayKey(i);
    }
#elif M5ATOM
    if (M5.BtnA.wasHold()) {
        byte i = audioPrevKey();
        serialcmdSend(255, 255);
        audioStop();
    }
    else if (M5.BtnA.wasClicked()) {
        byte i = audioNextKey();
        serialcmdSend(255, i);
        audioPlayKey(i);
    }
#endif

}
