
#include <usbh_midi.h>
#include <usbhub.h>

USB Usb;
USBHub Hub(&Usb);
USBH_MIDI  Midi(&Usb);

bool usbOK = false;

void MIDI_init()
{
  char buf[20];
  uint16_t vid = Midi.idVendor();
  uint16_t pid = Midi.idProduct();
  sprintf(buf, "VID:%04X, PID:%04X", vid, pid);
  Serial.println(buf); 
}


// Poll USB MIDI Controler and send to serial MIDI
void MIDI_poll()
{
  char buf[16];
  uint8_t bufMidi[MIDI_EVENT_PACKET_SIZE];
  uint16_t  rcvd;

  if (Midi.RecvData( &rcvd,  bufMidi) == 0 ) {
    // uint32_t time = (uint32_t)millis();
    // sprintf(buf, "%04X%04X:%3d:", (uint16_t)(time >> 16), (uint16_t)(time & 0xFFFF), rcvd); // Split variable to prevent warnings on the ESP8266 platform
    // Serial.print(buf);

    // for (int i = 0; i < MIDI_EVENT_PACKET_SIZE; i++) {
    //   sprintf(buf, " %02X", bufMidi[i]);
    //   Serial.print(buf);
    // }

    // TYPE
    if (bufMidi[0] == 0x09) {
      // Note On
      Serial.print(" Note On");
    } else if (bufMidi[0] == 0x08) {
      // Note Off
      Serial.print(" Note Off");
    } else if (bufMidi[0] == 0x0B) {
      // Control Change
      Serial.print(" Control Change");
    } else if (bufMidi[0] == 0x0E) {
      // Pitch Bend
      Serial.print(" Pitch Bend");
    } else if (bufMidi[0] == 0x0A) {
      // Polyphonic Aftertouch
      Serial.print(" Polyphonic Aftertouch");
    } else if (bufMidi[0] == 0x0C) {
      // Program Change
      Serial.print(" Program Change");
    } else if (bufMidi[0] == 0x0D) {
      // Channel Aftertouch
      Serial.print(" Channel Aftertouch");
    } else if (bufMidi[0] == 0x0F) {
      // System Exclusive
      Serial.print(" System Exclusive");
    } else {
      Serial.print(" Unknown");
    }

    // CHANNEL
    Serial.print(" Channel:");
    Serial.print(bufMidi[1] & 0x0F);

    // DATA
    Serial.print(" Data:");
    Serial.print(bufMidi[2]);
    Serial.print(" ");
    Serial.print(bufMidi[3]);



    Serial.println("");
  }
}

bool midiSetup()
{
  if (Usb.Init() == -1) {
    usbOK = false;
    Serial.println("USB init failed");
    return false;
  }

  Midi.attachOnInit(MIDI_init);

  usbOK = true;
  Serial.println("USB init ok");
  return true;
}

void midiLoop()
{
  if (!usbOK) return;

  Usb.Task();
  if ( Midi ) {
    MIDI_poll();
  }
}
