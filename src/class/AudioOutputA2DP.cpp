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

#define c3_frequency  130.81

BluetoothA2DPSource a2dp_source;

int32_t feedBT(Frame *frame, int32_t frame_count)
{
    static float m_time = 0.0;
    float m_amplitude = 10000.0;  // -32,768 to 32,767
    float m_deltaTime = 1.0 / 44100.0;
    float m_phase = 0.0;
    float pi_2 = PI * 2.0;
    // fill the channel data
    for (int sample = 0; sample < frame_count; ++sample) {
        float angle = pi_2 * c3_frequency * m_time + m_phase;
        frame[sample].channel1 = m_amplitude * sin(angle);
        frame[sample].channel2 = frame[sample].channel1;
        m_time += m_deltaTime;
    }
    // to prevent watchdog
    delay(1);

    return frame_count;
}

AudioOutputA2DP::AudioOutputA2DP()
{
//   buffSize = buffSizeSamples;
//   leftSample = (int16_t*)malloc(sizeof(int16_t) * buffSize);
//   rightSample = (int16_t*)malloc(sizeof(int16_t) * buffSize);
//   writePtr = 0;
//   readPtr = 0;
//   sink = dest;
    a2dp_source.set_auto_reconnect(true);
    a2dp_source.start("LEXON MINO L", feedBT);  
    a2dp_source.set_volume(30);
}

AudioOutputA2DP::~AudioOutputA2DP()
{
//   free(leftSample);
//   free(rightSample);
}

bool AudioOutputA2DP::begin()
{
  samples = 0;
  return true;
}

bool AudioOutputA2DP::ConsumeSample(int16_t sample[2])
{
  (void)sample; 
  samples++;
  Serial.printf("AudioOutputA2DP::ConsumeSample %d\n", samples);
  return true;
}



bool AudioOutputA2DP::stop()
{
  return true;
}


