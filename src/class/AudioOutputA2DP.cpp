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
#include "BluetoothA2DPSource.h"
#include <math.h> 

BluetoothA2DPSource a2dp_source;

int16_t buffer[A2DP_BUFFER_SIZE][2];
int readPtr = 0;
int writePtr = 0;

int32_t feedBT(Frame *frame, int32_t frame_count)
{ 
    int framedata = 0;

    int diff = (A2DP_BUFFER_SIZE + writePtr - readPtr) % A2DP_BUFFER_SIZE;
    while(diff < frame_count) {
      // Serial.printf("Not enough data to feed BT %d/%d\n", diff, frame_count);
      delay(1);
      diff = (A2DP_BUFFER_SIZE + writePtr - readPtr) % A2DP_BUFFER_SIZE;
    }

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

AudioOutputA2DP::AudioOutputA2DP()
{
    a2dp_source.set_auto_reconnect(false);
    a2dp_source.start("BoseMicro-1", feedBT);  
    a2dp_source.set_volume(10);
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


