
#include "USB_Host_Shield_2.0/usbh_midi.h"
#include "USB_Host_Shield_2.0/usbhub.h"

// TODO: FIX avrpins for modified shield

USB Usb;
USBHub Hub(&Usb);
USBH_MIDI  Midi(&Usb);

bool usbOk = false;
bool midiOk = false;

// Fifo midiStack using circular buffer
#define MIDI_STACK_SIZE 10
byte midiStack[MIDI_STACK_SIZE];
int midiStackHead = 0;
int midiStackTail = 0;

void MIDI_init()
{
  char buf[20];
  uint16_t vid = Midi.idVendor();
  uint16_t pid = Midi.idProduct();
  sprintf(buf, "VID:%d, PID:%d", vid, pid);
  Serial.println(buf); 

  midiOk = (vid == 2536 && pid == 76);
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

    byte cmd = 0;

    // TYPE
    if (bufMidi[0] == 0x09) {
      // Note On
      Serial.print(" Note On");
      if (bufMidi[2] == 40) cmd = 1;
      if (bufMidi[2] == 41) cmd = 2;
      if (bufMidi[2] == 42) cmd = 3;
      if (bufMidi[2] == 43) cmd = 4;
      if (bufMidi[2] == 36) cmd = 5;
      if (bufMidi[2] == 37) cmd = 6;
      if (bufMidi[2] == 38) cmd = 7;
      if (bufMidi[2] == 39) cmd = 8;
    } else if (bufMidi[0] == 0x08) {
      // Note Off
      Serial.print(" Note Off");
    } else if (bufMidi[0] == 0x0B) {
      // Control Change
      Serial.print(" Control Change");
      if (bufMidi[2] == 16 && bufMidi[3] > 0) cmd = 9;
      if (bufMidi[2] == 17 && bufMidi[3] > 0) cmd = 10;
      if (bufMidi[2] == 18 && bufMidi[3] > 0) cmd = 11;
      if (bufMidi[2] == 19 && bufMidi[3] > 0) cmd = 12;
      if (bufMidi[2] == 12 && bufMidi[3] > 0) cmd = 13;
      if (bufMidi[2] == 13 && bufMidi[3] > 0) cmd = 14;
      if (bufMidi[2] == 14 && bufMidi[3] > 0) cmd = 15;
      if (bufMidi[2] == 15 && bufMidi[3] > 0) cmd = 16;
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

    // midiStack incoming message
    if (cmd > 0) {
      midiStack[midiStackHead] = cmd;
      midiStackHead = (midiStackHead + 1) % MIDI_STACK_SIZE;
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
    usbOk = false;
    Serial.println("USB init failed");
    return false;
  }

  Midi.attachOnInit(MIDI_init);

  usbOk = true;
  Serial.println("USB init ok");
  return true;
}

void midiLoop()
{
  if (!usbOk) return;

  Usb.Task();
  if ( Midi ) {
    MIDI_poll();
  }
}

bool midiOK()
{
  return midiOk;
}


bool midiStackIsEmpty() 
{   
    return midiStackHead == midiStackTail;
}

byte midiStackPop() 
{
    byte outgoing = midiStack[midiStackTail];
    midiStackTail = (midiStackTail + 1) % MIDI_STACK_SIZE;
    return outgoing;
}