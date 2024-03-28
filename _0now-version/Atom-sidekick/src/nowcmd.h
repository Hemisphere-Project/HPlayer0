#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Fifo nowStack using circular buffer
#define NOW_STACK_SIZE 10
byte nowStack[NOW_STACK_SIZE];
int nowStackHead = 0;
int nowStackTail = 0;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t nowOffset = 0; 
bool nowOK = false;

unsigned long nowSyncLast = 0;

// nowMillis
uint32_t nowMillis() 
{
    return millis() + nowOffset;
}

// nowAdjust
void nowAdjust(uint32_t remoteMillis) 
{   
    uint32_t localMillis = nowMillis();
    int32_t diff = remoteMillis - localMillis;
    if (diff <= 0) return;

    nowOffset += diff;
    Serial.printf("Time sync: %d - delta %d\n", remoteMillis, diff);
}


void nowOnReceive(const uint8_t *mac, const uint8_t *data, int len) 
{
    // Parse
    uint32_t from = mac[5] | mac[4] << 8 | mac[3] << 16 | mac[2] << 24;
    String msg = String((char*)data).substring(0, len);

    // Serial.printf("-- Received from %u msg: %s\n", from, msg.c_str());

    // Cmd to stack
    if (len == 1) 
    {
        nowStack[nowStackHead] = data[0];
        nowStackHead = (nowStackHead + 1) % NOW_STACK_SIZE;
    }

    // Time sync
    else if (msg.startsWith("T=")) 
    {
        uint32_t remoteTime = strtoull(msg.substring(2).c_str(), NULL, 10);
        nowAdjust(remoteTime);
    }
}


// nowSetup
bool nowSetup()
{
    // Set ESP32 in STA mode to begin with
    WiFi.mode(WIFI_STA);

    // Print MAC address
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Disconnect from WiFi
    WiFi.disconnect();

    // Initialize ESP-NOW
    if (esp_now_init() == ESP_OK)
    {
        Serial.println("ESP-NOW Init Success");
        esp_now_register_recv_cb(nowOnReceive);
    }
    else
    {
        Serial.println("ESP-NOW Init Failed");
        nowOK = false;
        return false;
    }

    // Add Broadcast
    esp_now_peer_info_t peerInfo = {};
    memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
    if (!esp_now_is_peer_exist(broadcastAddress)) esp_now_add_peer(&peerInfo);

    nowOK = true;
    return true;
}

// nowStop
void nowStop() 
{
    nowOK = false;
    esp_now_unregister_recv_cb();
    esp_now_del_peer(broadcastAddress);
    esp_now_deinit();

    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
}


// nowBroadcast
void nowBroadcast(String msg) {
    if (!nowOK) return;
    esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)msg.c_str(), msg.length());
    // if (result == ESP_OK) {
    //     Serial.println("Sent with success");
    // }
    // else {
    //     Serial.println("Error sending the data");
    // }
}

void nowBroadcast(byte cmd) {
    if (!nowOK) return;
    esp_err_t result = esp_now_send(broadcastAddress, &cmd, 1);
    // if (result == ESP_OK) {
    //     Serial.println("Sent with success");
    // }
    // else {
    //     Serial.println("Error sending the data");
    // }
}

// nowLoop
void nowLoop() 
{
    if (!nowOK) return;

    // Time sync
    if (millis() - nowSyncLast > 1000) 
    {
        nowSyncLast = millis();
        nowBroadcast("T=" + String(nowMillis()));
    }
}

bool nowStackIsEmpty() 
{   
    return nowStackHead == nowStackTail;
}

byte nowStackPop() 
{
    byte outgoing = nowStack[nowStackTail];
    nowStackTail = (nowStackTail + 1) % NOW_STACK_SIZE;
    return outgoing;
}
