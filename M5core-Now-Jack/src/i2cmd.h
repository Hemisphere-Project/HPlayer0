#include <Arduino.h>
#include <Wire.h>

// Fifo i2cmdStack using circular buffer
#define I2CMD_STACK_SIZE 10
byte i2cmdStack[I2CMD_STACK_SIZE];
int i2cmdStackHead = 0;
int i2cmdStackTail = 0;

bool i2cmdOk = false;

void i2cmdRecv(int bytes) {
    while (Wire.available()) {
        byte cmd = Wire.read();
        // i2cmdStack incoming message
        i2cmdStack[i2cmdStackHead] = cmd;
        i2cmdStackHead = (i2cmdStackHead + 1) % I2CMD_STACK_SIZE;
    }
}

bool i2cmdSetup()
{
    Wire.begin(8);                  // join i2c bus with address #8
    Wire.onReceive(i2cmdRecv);   // call receiveEvent when data is received
    i2cmdOk = true;
    return true;
}


void i2cmdSend(byte cmd) 
{
    if (!i2cmdOk) return;

    Wire.beginTransmission(2); // transmit to device #0
    Wire.write(cmd);            // sends one byte
    Wire.endTransmission();    // stop transmitting
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

