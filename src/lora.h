#include <Arduino.h>
#include <M5LoRa.h>
#include <M5Unified.h>

// Fifo stack using circular buffer
#define STACK_SIZE 10
byte stack[STACK_SIZE];
int stackHead = 0;
int stackTail = 0;

void loraSend(byte cmd) 
{
    LoRa.beginPacket();        // start packet.  开始包
    LoRa.print(cmd, 0);           // add payload.  添加有效载荷
    LoRa.endPacket();  // finish packet and send it.  完成数据包并发送

    M5.Display.drawRightString( String(cmd), 10, 120);
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

