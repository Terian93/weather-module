#include "Arduino.h"
#include <LoRa.h>
#include <EEPROM.h>
#include "RTClib.h"

#define EMPTYID "0000"
#define NETWORK_TABLE_LENGTH 20

class Network {
  private:
    bool state = false;
    int syncWord = 0x13;
    String personalKey = "0002";
    String serverKey = "0001";
    String networkingTableKey[NETWORK_TABLE_LENGTH] = {};
    String networkingTableValue[NETWORK_TABLE_LENGTH] = {};
    String halfone;
    String halftwo;
    String memKey;

    void saveId(String id) {
      byte halfone = (byte) strtol(id.substring(0,2).c_str(), 0, 16);
      byte halftwo = (byte) strtol(id.substring(2,4).c_str(), 0, 16);
      EEPROM.update(0, halfone);
      EEPROM.update(1, halftwo);
    }
    
    String getId() {
      String halfone = String((int)EEPROM.read(0), HEX);
      String halftwo = String((int)EEPROM.read(1), HEX);
      return String((halfone.length() == 1 ? 0 + halfone : halfone) +
      (halftwo.length() == 1 ? 0 + halftwo : halftwo));
    }
  public:
    String sender;
    String reciever;
    String endReciever;
    String command;
    String msg;

    bool init(long seed) {
      state = LoRa.begin(433E6);
      if (!state) {
        Serial.println("020/-/ LoRa init failed!");
        return false;
      } else {
        LoRa.setSyncWord(syncWord);
        memKey = getId();
        if (personalKey != EMPTYID) {
          saveId(personalKey);
        } else if (personalKey == EMPTYID && memKey == EMPTYID) {
          randomSeed(seed);
          String key = String(random(2, 65536), 16); //equals to 4 digit hex;
          switch (key.length())
          {
            case 1:
              personalKey = "000" + key;
              break;
            case 2:
              personalKey = "00" + key;
              break;
            case 3:
              personalKey = "0" + key;
              break;

            default:
              personalKey = key;
              break;
          }
        } else if (memKey != EMPTYID) {
          personalKey = memKey;
        }
        Serial.print("021/-/ LoRa initialized with key:");
        Serial.println(String(personalKey));
        return true;
      }

    }

    void changePersonalKey(String key) {
      if (key.length() == 4) {
        personalKey = key;
        Serial.print("021/-/ LoRa changed key to:");
        Serial.println(String(personalKey));
      }
    }

    void idle() {
      LoRa.idle();
    }

    void sleep() {
      LoRa.sleep();
    }

    String getPersonalKey() {
      return personalKey;
    }

    String getServerKey() {
      return serverKey;
    }

    void sendMessage(
      String message,
      String reciever,
      String endReciever,
      String instruction
    ) {
//      Serial.println("data");
      LoRa.beginPacket();
      LoRa.print(personalKey);
      LoRa.print(reciever);
      LoRa.print(endReciever);
      LoRa.print(instruction);
      LoRa.print(message);
      LoRa.endPacket();
      Serial.println();
      Serial.print(reciever);
      Serial.print(':');
      Serial.print(personalKey);
      Serial.print(':');
      Serial.print(endReciever);
      Serial.print(':');
      Serial.print(instruction);
      Serial.print(':');
      Serial.println(message);
    }

    bool onRecieve() {
      if (LoRa.parsePacket() == 0) return false;

      String incomingMessage = "";

      while (LoRa.available()) {
        incomingMessage += (char)LoRa.read();
      }

      sender = incomingMessage.substring(0, 4);
      reciever = incomingMessage.substring(4, 8);
      endReciever = incomingMessage.substring(8, 12);
      command = incomingMessage.substring(12, 14);
      msg = incomingMessage.substring(14);

//      Serial.println("Message: " + sender + ":" + reciever + ":" + endReciever + ":" + command + ":" + msg);
//      Serial.println("RSSI: " + String(LoRa.packetRssi()));
//      Serial.println("Snr: " + String(LoRa.packetSnr()));
//      Serial.println();

      if (endReciever == personalKey && endReciever.length() == 4) {
        if (getPathToModule(serverKey) == -1 && !saveRoute(serverKey, sender)) {
          sendMessage(
            (String&)(personalKey + personalKey + "01"+"1"),
            sender,
            serverKey,
            (String&)"05"
          );
        }
        return true;
      } else if ( reciever == personalKey && endReciever.length() == 4 && getPathToModule(endReciever) > -1) {
        String newReciever = getReciever(endReciever);
        if (command == "01" && msg.length() == 4) {
          saveRoute(msg, newReciever);
        }
        sendMessage(msg, newReciever, endReciever, command);
      } else if (reciever == personalKey) {
        sendError((String&)"Cant find route");
      }
      return false;
    };

    int getPathToModule(String id) {
      for(int i = 0; i < NETWORK_TABLE_LENGTH; i++) {
        if(networkingTableKey[i] == id) {
          return i;
        }
      }
      return -1;
    }

    String getReciever(String id) {
      int index = getPathToModule(id);
      if (index > -1) {
        return networkingTableValue[index];
      } else {
        return serverKey;
      }
    }

    bool saveRoute(String key, String value) {
      if (key.length() == 4 && value.length() == 4) {
        for(int i = 0; i < NETWORK_TABLE_LENGTH; i++) {
          if(networkingTableKey[i].length() != 4 || networkingTableKey[i] == key) {
            networkingTableKey[i] = key;
            networkingTableValue[i] = value;
            return true;
          }
        }
      }
      return false;
    }

    void sendError(String message) {
      sendMessage(
        (String&)personalKey + endReciever + command + message,
        getReciever(serverKey),
        serverKey,
        "05"
      );
    }
};
