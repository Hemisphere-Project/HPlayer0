#include <Arduino.h>

#include <M5Unified.h>
#include <SD.h>

#include "lora.h"
#include "audio.h"
#include "usbmidi.h"
#include "menu.h"

int playingIndex = 0;


void setup(void) {

    Serial.begin(115200);
    Serial.println("Start");

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
    // LoRa.setPins(); 
    // while (!LoRa.begin(868E6)) M5.Display.drawCenterString("LoRa not found..", 160, 70);
    // M5.Display.drawCenterString("    LoRa ok !    ", 160, 70);
    // LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN);
    // LoRa.setSpreadingFactor(10);        // 6: faster - 12: stronger
    // LoRa.setSignalBandwidth(125E3);     // 7.8E3  10.4E3  15.6E3  20.8E3  31.25E3  41.7E3  62.5E3  125E3  250E3  500E3  bps
    // LoRa.setCodingRate4(8);             // 5: faster - 8: stronger

    // MIDI init
    if (!midiSetup()) M5.Display.drawCenterString("USB not found..", 160, 90);
    else M5.Display.drawCenterString("    USB ok !    ", 160, 90);

    delay(1000);

    // Interface
    // menu_init();

    // AUDIO init
    audioSetup();

}

void loop(void) 
{   
    // loraLoop();
    midiLoop();
    audioLoop();
    // menu_loop();

    while (!loraStackIsEmpty()) {
        byte cmd = loraStackPop();
        M5.Display.drawCenterString( String(cmd), 160, 90);

        if (cmd == 255) audioStop();
        else if (cmd < file_count) {
            audioPlay(filenames[cmd]);
            playingIndex = cmd;
        }
    }    
    
    

    // M5.update();
    
    // if (M5.BtnA.wasClicked()) {
    //     playingIndex = (playingIndex + file_count - 1) % file_count;
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

