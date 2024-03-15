#include <Arduino.h>

#include <M5Unified.h>
#include <Preferences.h>

#include <nowcmd.h>
#include <serialcmd.h>

Preferences preferences;


/////////////////////////////////////////////////////

void setup(void) {

    Serial.begin(115200);
    Serial.println("Start");

    M5.begin();

    // I2C Link
    serialcmdSetup(26, 32);

    // ESP Link (if not BT in use)
    nowSetup();
}

/////////////////////////////////////////////////////

void loop(void) 
{   
    nowLoop();
    serialcmdLoop();

    while (!nowStackIsEmpty()) {
        byte cmd = nowStackPop();
        serialcmdSend(cmd);
        Serial.println("Now->Serial: " + String(cmd));
    }

    while (!serialcmdStackIsEmpty()) {
        byte cmd = serialcmdStackPop();
        nowBroadcast(cmd);
        Serial.println("Serial->Now: " + String(cmd));
    }

    M5.update();
    
    if (M5.BtnA.wasPressed()) {
        Serial.println("A");
        serialcmdSend(0);
    }
}

