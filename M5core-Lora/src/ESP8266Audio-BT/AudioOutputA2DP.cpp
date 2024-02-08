/*
  AudioOutputA2DP
  Adds additional bufferspace to the output chain
  
  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include "AudioOutputA2DP.h"
#include <BluetoothA2DPSource.h>
#include <math.h> 

BluetoothA2DPSource a2dp_source;

int16_t buffer[A2DP_BUFFER_SIZE][2];
int readPtr = 0;
int writePtr = 0;

const char* SSID;

int32_t feedBT(Frame *frame, int32_t frame_count)
{ 

    int framedata = 0;

    // TODO: send 0 if not playing

    int diff = (A2DP_BUFFER_SIZE + writePtr - readPtr) % A2DP_BUFFER_SIZE;
    int needsWait = 0;

    while(diff < frame_count) 
    {
      // Serial.printf("Not enough data to feed BT %d/%d\n", diff, frame_count);
      delay(2);
      diff = (A2DP_BUFFER_SIZE + writePtr - readPtr) % A2DP_BUFFER_SIZE;
      needsWait++;

      // No data for 20ms, feed silence
      if (needsWait > 10) {
        frame[0].channel1 = 0;
        frame[0].channel2 = 0;
        // Serial.println("Not enough data to feed BT, sending silence..");
        return 1;
      }
    }
    // if (needsWait > 0) Serial.printf("Waited %d times\n", needsWait);

    // Serial.printf("Feed BT %d/%d\n", diff, frame_count);

    for (int sample = 0; sample < frame_count; ++sample) 
    {
      if (readPtr != writePtr) {
        frame[sample].channel1 = buffer[readPtr][0];
        frame[sample].channel2 = buffer[readPtr][1];
        readPtr = (readPtr + 1) % A2DP_BUFFER_SIZE;
        framedata++;
      } else {
        Serial.println("BT Buffer underrun");
        break;
      }
    }

    return frame_count;
}


AudioOutputA2DP::AudioOutputA2DP(const char* ssid)
{
    // compose name with ssid + "player"
    char name[32];
    sprintf(name, "%splayer", ssid);
    SSID = ssid;

    a2dp_source.set_local_name(name);
    a2dp_source.set_ssid_callback([](const char*_ssid, esp_bd_addr_t _address, int _rrsi) {
      Serial.printf("BT- Found %s RSSI %d Addr: %02x:%02x:%02x:%02x:%02x:%02x\n", _ssid, _rrsi, _address[0], _address[1], _address[2], _address[3], _address[4], _address[5]);
      Serial.printf("BT- Target SSID: %s\n", SSID);
      return (_ssid == SSID);
    });


    a2dp_source.set_discoverability(ESP_BT_NON_DISCOVERABLE);
    
    a2dp_source.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void* obj) {
      if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        Serial.println("BT- Connected");
      } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        Serial.println("BT- Disconnected");
      } else if (state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
        Serial.println("BT- Connecting ");
      } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTING) {
        Serial.println("BT- Disconnecting");
      } else {
        Serial.printf("BT- Unknown state: %d\n", state);
      }
    });

    // BM2 = c8:7b:23:ac:9f:d1

    // if (ssid == "BM1") {
    //     esp_bd_addr_t address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //     a2dp_source.set_auto_reconnect( false );
    //     a2dp_source.connect_to(address);
    // }
    // else if (ssid == "BM2") {
    //     esp_bd_addr_t address = {0xc8, 0x7b, 0x23, 0xac, 0x9f, 0xd1};
    //     a2dp_source.set_auto_reconnect( false );
    //     a2dp_source.connect_to(address);
    // }
    // else {
    //     esp_bd_addr_t address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //     a2dp_source.set_auto_reconnect( false );
    //     a2dp_source.connect_to(address);
    // } 

    // esp_bd_addr_t address = {0xc8, 0x7b, 0x23, 0xac, 0x9f, 0xd1};
    // a2dp_source.set_auto_reconnect( false );
    // a2dp_source.connect_to(address);

    a2dp_source.start(ssid, feedBT);  
    a2dp_source.set_volume(100);
}

AudioOutputA2DP::~AudioOutputA2DP()
{
}

bool AudioOutputA2DP::begin()
{
  samples = 0;
  filled = false;
  return true;
}

bool AudioOutputA2DP::SetGain(float f)
{
  if (f>1.0) f = 1.0;
  a2dp_source.set_volume(f*100);
  return true;
}

bool AudioOutputA2DP::ConsumeSample(int16_t sample[2])
{ 


  // Now, do we have space for a new sample?
  int nextWritePtr = (writePtr + 1) % A2DP_BUFFER_SIZE;
  if (nextWritePtr == readPtr) {
    filled = true;
    return false;
  }

  buffer[writePtr][0] = sample[0];
  buffer[writePtr][1] = sample[1];
  MakeSampleStereo16( buffer[writePtr] );
  writePtr = nextWritePtr;
  samples++;
  return true;

}

bool AudioOutputA2DP::stop()
{
  return true;
}

bool AudioOutputA2DP::isConnected()
{
  return a2dp_source.is_connected();
}


