//spiceapi
#define SPICEAPI_INTERFACE Serial
#define SPICEDEBUG_INTERFACE Serial1
#include "connection.h"
#include "wrappers.h"

//pn5180
#include "PN5180.h"
#include "PN5180FeliCa.h"
#include "PN5180ISO15693.h"

//CardReader
#define PN5180_NSS 48
#define PN5180_BUSY 49
#define PN5180_RST 53

//LED
#include <FastLED.h>
#define REDPIN   11
#define GREENPIN 12
#define BLUEPIN  13

//spiceapi con
spiceapi::Connection CON(4096, "");
void setup() {
  SPICEAPI_INTERFACE.begin(115200);
  while (!SPICEAPI_INTERFACE);
  //2nd ttl for debug
  SPICEDEBUG_INTERFACE.begin(9600);

  pinMode(REDPIN,   OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN,  OUTPUT);
}

void loop() {
  //RgbLeds();
  CardReader();
  Headphones();

  //delay(20);
}
void ttldebug() {
  int temp = SPICEAPI_INTERFACE.read();
  SPICEDEBUG_INTERFACE.write(temp);
}

void RgbLeds() {
  int r = 0, g = 0, b = 0;

  //req the light state
  /*String req = "{\"id\":"+String((int)spiceapi::msg_gen_id())+",\"module\":\"lights\",\"function\":\"read\",\"params\":[]}";
  auto res = CON.request(req);
  delete req;
  //parse the string to object
  DynamicJsonDocument *obj = new DynamicJsonDocument(2048);
  deserializeJson(*obj, res);
  SPICEDEBUG_INTERFACE.println(spiceapi::doc2str(obj));
  auto data = (*obj)["data"].as<JsonArray>();
  int count = 0;
  for (auto val : data) {
    if (val[0] == "Wing Left Up R") {
      r = (int) (255 - 254 * val[1].as<float>());
    } else if (val[0] == "Wing Left Up G") {
      g = (int) (255 - 254 * val[1].as<float>());
    } else if (val[0] == "Wing Left Up B") {
      b = (int) (255 - 254 * val[1].as<float>());
    }
    count++;
  }
  SPICEDEBUG_INTERFACE.println(count);

  analogWrite(REDPIN, r);
  analogWrite(GREENPIN, g);
  analogWrite(BLUEPIN, b);*/
}



void Headphones() {
  CON.request("{\"id\":3,\"module\":\"buttons\",\"function\":\"write\",\"params\":[[\"Headphone\",1.0]]}");
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
            SPICEDEBUG_INTERFACE.println(num);
            String req = "{\"id\":2,\"module\":\"card\",\"function\":\"insert\",\"params\":[[0,\"" + num + "\"]]}";
            CON.request(req.c_str());
            //spiceapi::card_insert(CON, 0, num.c_str());//job done


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
          SPICEDEBUG_INTERFACE.println(num);
          String req = "{\"id\":2,\"module\":\"card\",\"function\":\"insert\",\"params\":[[0,\"" + num + "\"]]}";
          CON.request(req.c_str());
          //spiceapi::card_insert(CON, 0, num.c_str());
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
