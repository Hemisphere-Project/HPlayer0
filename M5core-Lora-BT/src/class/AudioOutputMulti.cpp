/*
  AudioOutputMulti
  Adds additional bufferspace to the output chain
  
  Copyright (C) 2023, Thomas BOHL

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
#include "AudioOutputMulti.h"



AudioOutputMulti::AudioOutputMulti(AudioOutput *dest1, AudioOutput *dest2)
{
  sink1 = dest1;
  sink2 = dest2;

  buffer1.readPtr = 0;
  buffer2.readPtr = 0;
}

bool AudioOutputMulti::SetRate(int hz)
{
  return sink1->SetRate(hz) && sink2->SetRate(hz);
}

bool AudioOutputMulti::SetBitsPerSample(int bits)
{
  return sink1->SetBitsPerSample(bits) && sink2->SetBitsPerSample(bits);
}

bool AudioOutputMulti::SetChannels(int channels)
{
  return sink1->SetChannels(channels) && sink2->SetChannels(channels);
}

bool AudioOutputMulti::begin()
{
  buffer1.available = 0;
  buffer2.available = 0;
  return sink1->begin() && sink2->begin();
}

bool AudioOutputMulti::ConsumeSample(int16_t sample[2])
{

  // Feed sink1 with sample
  // if (!sink1->ConsumeSample(sample)) return false;
  
  // Feed sink2 with sample (sample are discarded if full)
  return sink2->ConsumeSample(sample);

  return true;

  // // try to feed sink1 with exisiting values
  // while (buffer1.available > 0) {
  //   int16_t s[2] = {buffer1.left[buffer1.readPtr], buffer1.right[buffer1.readPtr]};
  //   if (!sink1->ConsumeSample(s)) break; // Can't stuff any more in I2S...
  //   buffer1.readPtr = (buffer1.readPtr + 1) % MULTI_BUFSIZE;
  //   buffer1.available--;
  // }

  // // try to feed sink2 with exisiting values
  // while (buffer2.available > 0) {
  //   int16_t s[2] = {buffer2.left[buffer2.readPtr], buffer2.right[buffer2.readPtr]};
  //   if (!sink2->ConsumeSample(s)) break; // Can't stuff any more in I2S...
  //   buffer2.readPtr = (buffer2.readPtr + 1) % MULTI_BUFSIZE;
  //   buffer2.available--;
  // }

  // // Do we have space for a new sample?
  // if (buffer1.available == MULTI_BUFSIZE || buffer2.available == MULTI_BUFSIZE) 
  //   return false;

  // // Store the sample in buffer1
  // int nextWritePtr = (buffer1.readPtr + 1) % MULTI_BUFSIZE;
  // buffer1.left[nextWritePtr] = sample[LEFTCHANNEL];
  // buffer1.right[nextWritePtr] = sample[RIGHTCHANNEL];
  // buffer1.available += 1;

  // // Store the sample in buffer2
  // nextWritePtr = (buffer2.readPtr + 1) % MULTI_BUFSIZE;
  // buffer2.left[nextWritePtr] = sample[LEFTCHANNEL];
  // buffer2.right[nextWritePtr] = sample[RIGHTCHANNEL];
  // buffer2.available += 1;

  // return true;
}

bool AudioOutputMulti::stop()
{
  buffer1.readPtr = 0;
  buffer1.available = 0;
  buffer2.readPtr = 0;
  buffer2.available = 0;

  return sink1->stop() && sink2->stop();
}


