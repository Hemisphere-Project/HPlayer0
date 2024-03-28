#include <Arduino.h>
#include <M5Unified.h>
#include "M5LoRa.h"

bool loraOk = false;

// callaback
void (*loraRecv) (byte dest, byte cmd) = NULL;

bool loraSetup( void (*f)(byte dest, byte cmd) = NULL ) 
{
    // callback
    loraRecv = f;

    #if M5CORE
    // LORA init 868MHz
    LoRa.setPins(); 
    if (!LoRa.begin(868E6)) {
        loraOk = false;
        return false;
    } 
    LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN);            // 10 - 125 - 8
    LoRa.setSpreadingFactor(10);        // 6: faster - 12: stronger
    LoRa.setSignalBandwidth(125E3);     // 7.8E3  10.4E3  15.6E3  20.8E3  31.25E3  41.7E3  62.5E3  125E3  250E3  500E3  bps
    LoRa.setCodingRate4(8);             // 5: faster - 8: stronger
    loraOk = true;
    return true;
    #endif

    // not supported
    loraOk = false;
    return false;
}


bool loraLoop() 
{
    if (!loraOk) return false;

    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return false;  // no packet

    Serial.println("Lora IN: size="+String(packetSize));
    byte dest = (packetSize == 2) ? LoRa.read() : 255;  // first byte is the destination.
    byte cmd  = LoRa.read();                            // second byte is the command.

    // callaback
    if (loraRecv != NULL) loraRecv(dest, cmd);

    return true;
}

bool loraSend(byte dest, byte cmd) 
{
    if (!loraOk) return false;

    LoRa.beginPacket();         // start packet.  
    if (dest != 255) 
        LoRa.print(dest, 0);    // add destination.  
    LoRa.print(cmd, 0);         // add payload.  
    LoRa.endPacket();           // finish packet and send it. 
    
    return true;
}


