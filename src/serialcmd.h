#include <Arduino.h>

bool serialcmdOk = false;

// callback
void (*serialRecv) (byte dest, byte cmd) = NULL;

bool serialcmdSetup(int rx, int tx, void (*f)(byte dest, byte cmd)) 
{   
    Serial1.begin(115200, SERIAL_8N1, rx, tx);
    Serial.println("Serial1.begin");
    serialRecv = f;
    serialcmdOk = true;
    return true;
}

void serialcmdLoop() 
{
    int dest = -1;
    int cmd = -1;
    if (!serialcmdOk) return;
    while (Serial1.available())
    {
        byte b = Serial1.read();
        
        // B = 0 => cmd value OR end of packet
        if (b == 0) 
        {
            if (dest == -1) continue; // no dest 0 => means it's either cmd or end of packet => ignore
            else if (cmd == -1) cmd = b;
            else {
                // end of packet
                if (serialRecv != NULL && dest > 0 && cmd >= 0) serialRecv(dest, cmd);
                dest = -1;
                cmd = -1;
            }
        }
        else if (dest == -1) dest = b;
        else if (cmd == -1) cmd = b;
        else {
            // too many values.. wait for end of packet..
            dest = 0;
            cmd = 0;
        }
    }
}


void serialcmdSend(byte dest, byte cmd) 
{
    if (!serialcmdOk) return;
    Serial1.write(dest);
    Serial1.write(cmd);
    Serial1.write(0);
    Serial1.flush();
    Serial.println("Serial1.write");
}


