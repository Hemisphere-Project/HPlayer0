
#include "USB_Host_Shield_2.0/usbh_midi.h"
#include "USB_Host_Shield_2.0/usbhub.h"

// WARNING: FIX avrpins for modified shield
// USB_Host_Shield_2.0 / avrpins.h (line ~1697)
//
// Pinout for ESP32 dev module 
// //MAKE_PIN(P22, 22); // SCL // <-- COMMENTED OUT
// MAKE_PIN(P5, 22); // SS  // <-- MODIFIED

USB Usb;
USBHub Hub(&Usb);
USBH_MIDI  Midi(&Usb);

bool usbOk = false;
bool midiOk = false;

// callback
void (*midiRcv)(byte dest, byte cmd) = NULL;

void MIDI_init()
{
  char buf[20];
  uint16_t vid = Midi.idVendor();
  uint16_t pid = Midi.idProduct();
  sprintf(buf, "VID:%d, PID:%d", vid, pid);
  Serial.println(buf); 

  midiOk = (vid == 2536 && pid == 76);  // AKAI LPD8
}


// Poll USB MIDI Controler and send to serial MIDI
void MIDI_poll()
{
  char buf[16];
  uint8_t bufMidi[MIDI_EVENT_PACKET_SIZE];
  uint16_t  rcvd;

  while (Midi.RecvData( &rcvd,  bufMidi) == 0 ) 
  {
    byte cmd = 0;
    byte dest = 255;

    // TYPE
    if (bufMidi[0] == 0x09) {
      // Note On
      Serial.print(" Note On");

      // MEDIA 1->8
      if (bufMidi[2] == 40) cmd = 1;
      if (bufMidi[2] == 41) cmd = 2;
      if (bufMidi[2] == 42) cmd = 3;
      if (bufMidi[2] == 43) cmd = 4;
      if (bufMidi[2] == 36) cmd = 5;
      if (bufMidi[2] == 37) cmd = 6;
      if (bufMidi[2] == 38) cmd = 7;
      if (bufMidi[2] == 39) cmd = 255; // stop

    } else if (bufMidi[0] == 0x08) {
      // Note Off
      Serial.print(" Note Off");
    } else if (bufMidi[0] == 0x0B) {
      // Control Change
      Serial.print(" Control Change");
      
      // MEDIA 9->16
      if (bufMidi[2] == 16 && bufMidi[3] > 0) cmd = 9;
      if (bufMidi[2] == 17 && bufMidi[3] > 0) cmd = 10;
      if (bufMidi[2] == 18 && bufMidi[3] > 0) cmd = 11;
      if (bufMidi[2] == 19 && bufMidi[3] > 0) cmd = 12;
      if (bufMidi[2] == 12 && bufMidi[3] > 0) cmd = 13;
      if (bufMidi[2] == 13 && bufMidi[3] > 0) cmd = 14;
      if (bufMidi[2] == 14 && bufMidi[3] > 0) cmd = 15;
      if (bufMidi[2] == 15 && bufMidi[3] > 0) cmd = 255; // stop

      // VOLUMES
      if (bufMidi[2] >= 70 && bufMidi[2] <= 77) {
        cmd = 100+bufMidi[3];
        dest = bufMidi[2]-69;
      }

    } else if (bufMidi[0] == 0x0E) {
      // Pitch Bend
      Serial.print(" Pitch Bend");
    } else if (bufMidi[0] == 0x0A) {
      // Polyphonic Aftertouch
      Serial.print(" Polyphonic Aftertouch");
    } else if (bufMidi[0] == 0x0C) {
      // Program Change
      Serial.print(" Program Change");

      // MEDIA 17->24
      if (bufMidi[2] == 4 && bufMidi[3] > 0) cmd = 17;
      if (bufMidi[2] == 5 && bufMidi[3] > 0) cmd = 18;
      if (bufMidi[2] == 6 && bufMidi[3] > 0) cmd = 19;
      if (bufMidi[2] == 7 && bufMidi[3] > 0) cmd = 20;
      if (bufMidi[2] == 0 && bufMidi[3] > 0) cmd = 21;
      if (bufMidi[2] == 1 && bufMidi[3] > 0) cmd = 22;
      if (bufMidi[2] == 2 && bufMidi[3] > 0) cmd = 23;
      if (bufMidi[2] == 3 && bufMidi[3] > 0) cmd = 255; // stop

    } else if (bufMidi[0] == 0x0D) {
      // Channel Aftertouch
      Serial.print(" Channel Aftertouch");
    } else if (bufMidi[0] == 0x0F) {
      // System Exclusive
      Serial.print(" System Exclusive");
    } else {
      Serial.print(" Unknown");
    }

    // process msgs
    if (cmd > 0 && midiRcv != NULL) midiRcv(dest, cmd);

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

bool midiSetup( void (*f)(byte dest, byte cmd) )
{ 
  // callback
  midiRcv = f;

  #if M5CORE
  if (Usb.Init() == -1) {
    usbOk = false;
    Serial.println("USB init failed");
    return false;
  }

  Midi.attachOnInit(MIDI_init);

  usbOk = true;
  Serial.println("USB init ok");
  return true;
  #endif

  // not suppoted
  usbOk = false;
  return false;
}

bool midiOK()
{
  return midiOk;
}

bool usbOK()
{
  return usbOk;
}

void midiLoop()
{
  if (!usbOk) return;

  Usb.Task();
  if ( Midi ) {
    MIDI_poll();
  }
}