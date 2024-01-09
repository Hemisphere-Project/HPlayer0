#include <Arduino.h>
#include <M5Unified.h>
#include "class/M5LoRa.h"

// Fifo loraStack using circular buffer
#define LORA_STACK_SIZE 10
byte loraStack[LORA_STACK_SIZE];
int loraStackHead = 0;
int loraStackTail = 0;

bool loraOk = false;

bool loraSetup()
{
    // LORA init 868MHz
    LoRa.setPins(); 
    if (!LoRa.begin(868E6)) {
        loraOk = false;
        return false;
    } 
    LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN);
    LoRa.setSpreadingFactor(10);        // 6: faster - 12: stronger
    LoRa.setSignalBandwidth(125E3);     // 7.8E3  10.4E3  15.6E3  20.8E3  31.25E3  41.7E3  62.5E3  125E3  250E3  500E3  bps
    LoRa.setCodingRate4(8);             // 5: faster - 8: stronger
    loraOk = true;
    return true;
}


bool loraLoop() 
{
    if (!loraOk) return false;

    int packetSize = LoRa.parsePacket();
    if (packetSize == 0)
        return false;  // if there's no packet, return.  如果没有包，返回。

    // read packet header bytes:
    byte cmd         = LoRa.read(); // first byte is the command.  第一个字节是命令。

    // M5.Lcd.setTextColor(YELLOW);
    // M5.Lcd.println("Rcv: " + incoming);

    // loraStack incoming message
    loraStack[loraStackHead] = cmd;
    loraStackHead = (loraStackHead + 1) % LORA_STACK_SIZE;

    M5.Display.drawString( "Lora IN: "+String(cmd)+"    ", 10, 170);

    return true;
}

void loraSend(byte cmd) 
{
    if (!loraOk) return;

    LoRa.beginPacket();        // start packet.  开始包
    LoRa.print(cmd, 0);           // add payload.  添加有效载荷
    LoRa.endPacket();  // finish packet and send it.  完成数据包并发送

    M5.Display.drawString( "Lora OUT: "+String(cmd)+"    ", 10, 150);
}

bool loraStackIsEmpty() 
{   
    return loraStackHead == loraStackTail;
}

byte loraStackPop() 
{
    byte outgoing = loraStack[loraStackTail];
    loraStackTail = (loraStackTail + 1) % LORA_STACK_SIZE;
    return outgoing;
}

