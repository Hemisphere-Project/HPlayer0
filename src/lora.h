#include <Arduino.h>
#include <M5LoRa.h>
#include <M5Unified.h>

byte msgCount = 0;  // count of outgoing messages
byte localAddress = 0xBB;  // address of this device
byte destination  = 0xFF;  // destination to send to


void loraSend(String outgoing) 
{
    LoRa.beginPacket();        // start packet.  开始包
    LoRa.write(destination);          // destinataion 0xFF = broadcast
    LoRa.write(localAddress);          // add sender address.  添加发件人地址
    LoRa.write(msgCount);      // add message ID.  添加消息标识
    LoRa.write(outgoing.length());  // add payload length.  添加有效载荷长度
    LoRa.print(outgoing);           // add payload.  添加有效载荷
    LoRa.endPacket();  // finish packet and send it.  完成数据包并发送
    msgCount++;        // increment message ID.  增加消息 ID
    M5.Lcd.setTextColor(BLUE);
    M5.Lcd.println("Sending " + outgoing);
}

void loraLoop() 
{
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0)
        return;  // if there's no packet, return.  如果没有包，返回。

    // read packet header bytes:
    int recipient      = LoRa.read();  // recipient address.  收件人地址。
    byte sender        = LoRa.read();  // sender address.  发件人地址。
    byte incomingMsgId = LoRa.read();  // incoming msg ID.  传入的消息 ID。
    byte incomingLength = LoRa.read();  // incoming msg length.  传入消息长度。

    String incoming = "";

    while (LoRa.available()) {
        incoming += (char)LoRa.read();
    }

    if (incomingLength !=
        incoming.length()) {  // check length for error.  检查错误长度
        Serial.println("error: message length does not match length");
        return;  // skip rest of function.  跳过其余功能
    }

    // if the recipient isn't this device or broadcast,.
    // 如果收件人不是此设备或广播，
    if (recipient != localAddress && recipient != 0xFF) {
        Serial.println("This message is not for me.");
        return;  // skip rest of function.  跳过其余功能
    }

    // if message is for this device, or broadcast, print details:.
    // 如果消息是针对此设备或广播的，则打印详细信息:
    Serial.println("Received from: 0x" + String(sender, HEX));
    Serial.println("Sent to: 0x" + String(recipient, HEX));
    Serial.println("Message ID: " + String(incomingMsgId));
    Serial.println("Message length: " + String(incomingLength));
    Serial.println("Message: " + incoming);
    Serial.println("RSSI: " + String(LoRa.packetRssi()));
    Serial.println("Snr: " + String(LoRa.packetSnr()));
    Serial.println();

    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.println("Message: " + incoming);
}