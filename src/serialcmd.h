/*
    HPlayer0 - HPlayer0 is a modular media player designed for ESP32.
    Copyright (C) 2024 Thomas BOHL - thomas@37m.gr

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

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


