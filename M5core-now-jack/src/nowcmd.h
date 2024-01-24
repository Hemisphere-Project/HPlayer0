#include <Arduino.h>
#include <Wire.h>

// Fifo nowcmdStack using circular buffer
#define NOWCMD_STACK_SIZE 10
byte nowcmdStack[NOWCMD_STACK_SIZE];
int nowcmdStackHead = 0;
int nowcmdStackTail = 0;

bool nowcmdOk = false;

void nowcmdRecv(int bytes) {
    while (Wire.available()) {
        byte cmd = Wire.read();
        // nowcmdStack incoming message
        nowcmdStack[nowcmdStackHead] = cmd;
        nowcmdStackHead = (nowcmdStackHead + 1) % NOWCMD_STACK_SIZE;
    }
}

bool nowcmdSetup()
{
    Wire.begin(8);                  // join i2c bus with address #8
    Wire.onReceive(nowcmdRecv);   // call receiveEvent when data is received
    nowcmdOk = true;
    return true;
}

bool nowcmdLoop()
{
    if (!nowcmdOk) return false;
    
    return true;
}


void nowcmdSend(byte cmd) 
{
    if (!nowcmdOk) return;

    Wire.beginTransmission(2); // transmit to device #0
    Wire.write(cmd);            // sends one byte
    Wire.endTransmission();    // stop transmitting
}

bool nowcmdStackIsEmpty() 
{   
    return nowcmdStackHead == nowcmdStackTail;
}

byte nowcmdStackPop() 
{
    byte outgoing = nowcmdStack[nowcmdStackTail];
    nowcmdStackTail = (nowcmdStackTail + 1) % NOWCMD_STACK_SIZE;
    return outgoing;
}

