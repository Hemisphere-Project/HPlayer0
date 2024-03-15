#include <Arduino.h>

// Fifo serialcmdStack using circular buffer
#define SERIALCMD_STACK_SIZE 10
byte serialcmdStack[SERIALCMD_STACK_SIZE];
int serialcmdStackHead = 0;
int serialcmdStackTail = 0;

bool serialcmdOk = false;

bool serialcmdSetup(int rx, int tx)
{   
    Serial1.begin(115200, SERIAL_8N1, rx, tx);
    Serial.println("Serial1.begin");
    serialcmdOk = true;
    return true;
}

void serialcmdLoop() 
{
    if (!serialcmdOk) return;
    while (Serial1.available())
    {
        byte cmd = Serial1.read();
        // serialcmdStack incoming message
        serialcmdStack[serialcmdStackHead] = cmd;
        serialcmdStackHead = (serialcmdStackHead + 1) % SERIALCMD_STACK_SIZE;
    }
}


void serialcmdSend(byte cmd) 
{
    if (!serialcmdOk) return;
    Serial1.write(cmd);
    Serial.println("Serial1.write");
}

bool serialcmdStackIsEmpty() 
{   
    return serialcmdStackHead == serialcmdStackTail;
}

byte serialcmdStackPop() 
{
    byte outgoing = serialcmdStack[serialcmdStackTail];
    serialcmdStackTail = (serialcmdStackTail + 1) % SERIALCMD_STACK_SIZE;
    return outgoing;
}

