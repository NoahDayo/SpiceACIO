//spiceapi
#define SPICEAPI_INTERFACE Serial
#define SPICEDEBUG_INTERFACE Serial1
#include "connection.h"
#include "wrappers.h"

//pn5180
#include "PN5180.h"
#include "PN5180FeliCa.h"
#include "PN5180ISO15693.h"

//pin config
#define PN5180_NSS 48
#define PN5180_BUSY 49
#define PN5180_RST 53

//LED
#include <FastLED.h>
#define REDPIN   5
#define GREENPIN 6
#define BLUEPIN  7

//spiceapi con
spiceapi::Connection CON(4096, "");
void setup() {
  SPICEAPI_INTERFACE.begin(2000000);
  //2nd ttl for debug
  SPICEDEBUG_INTERFACE.begin(115200);

  pinMode(REDPIN,   OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN,  OUTPUT);

  //default lights
  analogWrite(REDPIN, 0);
  analogWrite(GREENPIN, 0);
  analogWrite(BLUEPIN, 0);
}

void loop() {
  RgbLeds();
  CardReader();
  Headphones();

  delay(50);
}
void ttldebug() {
  int temp = SPICEAPI_INTERFACE.read();
  SPICEDEBUG_INTERFACE.write(temp);
}

void RgbLeds() {
  int r = 0, g = 0, b = 0;
  spiceapi::LightState lights[35];
  int lights_size = spiceapi::lights_read(CON, lights, 35);
  for (int i = 0; i < lights_size; i++) {
    auto &light = lights[i];
    if (light.name.equals("Wing Left Up R"))
    {
      r = (int) 255 - 255 * light.value;
    }
    else if (light.name.equals("Wing Left Up G"))
    {
      g = (int) 255 - 255 * light.value;
    }
    else if (light.name.equals("Wing Left Up B"))
    {
      b = (int) 255 - 255 * light.value;
    }
  }
  SPICEDEBUG_INTERFACE.println(lights_size);

  analogWrite(REDPIN, r);
  analogWrite(GREENPIN, g);
  analogWrite(BLUEPIN, b);
}



void Headphones() {
  spiceapi::ButtonState buttons[1];
  buttons[0].name = "Headphone";
  buttons[0].value = 1.0;

  spiceapi::buttons_write(CON, buttons, 1);
}

void KeyPads() {
  spiceapi::keypads_write(CON, 0, "1234");
}

byte readstatus = 0;
uint8_t rdpn[8] = {0, 0, 0, 0, 0, 0, 0, 0};
byte uid[8];
byte card = 0;
char hex_table[17] = "0123456789ABCDEF";

void CardReader() {
  PN5180FeliCa nfcFeliCa(PN5180_NSS, PN5180_BUSY, PN5180_RST);
  PN5180ISO15693 nfc15693(PN5180_NSS, PN5180_BUSY, PN5180_RST);

  nfcFeliCa.begin();
  nfcFeliCa.reset();

  switch (readstatus)
  {
    case 0: //look for ISO15693(ePass)
      {
        nfc15693.reset();
        nfc15693.setupRF();

        //read ISO15693 inventory
        ISO15693ErrorCode rc = nfc15693.getInventory(rdpn);
        if (rc == ISO15693_EC_OK )
        {
          for (int i = 0; i < 8; i++) //fix uid as ISO15693 protocol sends data backwards
          {
            uid[i] = rdpn[7 - i];
          }

          if (uid[0] == 0xE0 && uid[1] == 0x04) // if correct konami card, job is done
          {
            card = 1; //iso15693
            char hex_str[16];
            for (int i = 0; i < 8; i++)
            {
              unsigned char _by = uid[i];
              hex_str[2 * i] = hex_table[(_by >> 4) & 0x0F];
              hex_str[2 * i + 1] = hex_table[_by & 0x0F];
            }
            String num;
            for (int i = 0; i < 16; i++) {
              num += hex_str[i];
            }
            //SPICEAPI_INTERFACE.println(num);
            spiceapi::card_insert(CON, 0, num.c_str());//job done
            readstatus = 0;
          }
          else //tag found but bad ID
          {
            readstatus = 1; // try to find a FeliCa
          }
        }
        else //tag not found
        {
          readstatus = 1; // try to find a FeliCa
        }
        break;
      }


    case 1: //look for FeliCa(AIC)
      {
        nfcFeliCa.reset();
        nfcFeliCa.setupRF();
        uint8_t uidLength = nfcFeliCa.readCardSerial(rdpn);

        if (uidLength > 0) //tag found
        {
          for (int i = 0; i < 8; i++)
          {
            uid[i] = rdpn[i];
          }
          card = 2; //felica

          //SPICEAPI_INTERFACE.println("Felica");
          char hex_str[16];
          for (int i = 0; i < 8; i++)
          {
            unsigned char _by = uid[i];
            hex_str[2 * i] = hex_table[(_by >> 4) & 0x0F];
            hex_str[2 * i + 1] = hex_table[_by & 0x0F];
          }
          String num;
          for (int i = 0; i < 16; i++) {
            num += hex_str[i];
          }
          //SPICEAPI_INTERFACE.println(num);
          spiceapi::card_insert(CON, 0, num.c_str());
          //job done

          readstatus = 0;
        }
        else //tag not found
        {
          card = 0;
          //we tried both protocols and found nothing, job done
          readstatus = 0;
        }
        break;
      }
  }
}
