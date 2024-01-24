#include <Arduino.h>
#include <Wire.h>

// Fifo i2cmdStack using circular buffer
#define I2CMD_STACK_SIZE 10
byte i2cmdStack[I2CMD_STACK_SIZE];
int i2cmdStackHead = 0;
int i2cmdStackTail = 0;

enum i2c_Mode { I2C_PLAYER=8, I2C_SIDEKICK=2 };

i2c_Mode i2cmode = I2C_PLAYER;

bool i2cmdOk = false;

void i2cmdRecv(int bytes) {
    while (Wire.available()) {
        byte cmd = Wire.read();
        // i2cmdStack incoming message
        i2cmdStack[i2cmdStackHead] = cmd;
        i2cmdStackHead = (i2cmdStackHead + 1) % I2CMD_STACK_SIZE;
    }
}

bool i2cmdSetup( i2c_Mode mode )
{   
    i2cmode = mode;
    Wire.begin(i2cmode);          // join i2c bus with address #8

    // Player = Receiver // Sidekick = Sender
    if (i2cmode == I2C_PLAYER) {
        Wire.onReceive(i2cmdRecv);   // call receiveEvent when data is received
    }

    i2cmdOk = true;
    return true;
}

void i2cmdLoop() 
{
    if (!i2cmdOk) return;
}


void i2cmdSend(byte cmd) 
{
    if (!i2cmdOk) return;
    if (i2cmode == I2C_PLAYER) return; // Player is receiver only (no send

    int i2cTargetAddress = i2cmode == I2C_PLAYER ? I2C_SIDEKICK : I2C_PLAYER;
    Wire.beginTransmission(i2cTargetAddress);   // transmit to device #2
    Wire.write(cmd);                            // sends one byte
    Wire.endTransmission();                     // stop transmitting
}

bool i2cmdStackIsEmpty() 
{   
    return i2cmdStackHead == i2cmdStackTail;
}

byte i2cmdStackPop() 
{
    byte outgoing = i2cmdStack[i2cmdStackTail];
    i2cmdStackTail = (i2cmdStackTail + 1) % I2CMD_STACK_SIZE;
    return outgoing;
}

