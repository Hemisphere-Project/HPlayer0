#include <Arduino.h>
#include <M5Unified.h>
#include "class/M5LoRa.h"

// Fifo stack using circular buffer
#define STACK_SIZE 10
byte stack[STACK_SIZE];
int stackHead = 0;
int stackTail = 0;

bool loraSetup()
{
    // LORA init 868MHz
    LoRa.setPins(); 
    if (!LoRa.begin(868E6)) return false;
    LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN);
    LoRa.setSpreadingFactor(10);        // 6: faster - 12: stronger
    LoRa.setSignalBandwidth(125E3);     // 7.8E3  10.4E3  15.6E3  20.8E3  31.25E3  41.7E3  62.5E3  125E3  250E3  500E3  bps
    LoRa.setCodingRate4(8);             // 5: faster - 8: stronger
    return true;
}


bool loraLoop() 
{
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0)
        return false;  // if there's no packet, return.  如果没有包，返回。

    // read packet header bytes:
    byte cmd         = LoRa.read(); // first byte is the command.  第一个字节是命令。

    // M5.Lcd.setTextColor(YELLOW);
    // M5.Lcd.println("Rcv: " + incoming);

    // stack incoming message
    stack[stackHead] = cmd;
    stackHead = (stackHead + 1) % STACK_SIZE;

    M5.Display.drawRightString( String(cmd), 300, 120);

    return true;
}

void loraSend(byte cmd) 
{
    LoRa.beginPacket();        // start packet.  开始包
    LoRa.print(cmd, 0);           // add payload.  添加有效载荷
    LoRa.endPacket();  // finish packet and send it.  完成数据包并发送

    M5.Display.drawString( String(cmd), 10, 120);
}

bool loraStackIsEmpty() 
{
    return stackHead == stackTail;
}

byte loraStackPop() 
{
    byte outgoing = stack[stackTail];
    stackTail = (stackTail + 1) % STACK_SIZE;
    return outgoing;
}

