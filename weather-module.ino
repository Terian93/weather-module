#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_BME280.h>
#include <EEPROM.h>

#define SELECTION 10
#define SENSORS 3

#define SEALEVELPRESSURE_HPA (1013.25)

class RTC {
  private:
    RTC_DS3231 core;
    DateTime alarm;
  public:
    bool initialized = false;
    bool init() {
      bool status = core.begin();      
      if (!status) {
        Serial.println("- Starting RTC failed!");
      } else {
        Serial.println("011/-/ - RTC started successfully");
        if (core.lostPower()) {
          Serial.println("012/-/ RTC lost power, let's set the time!");
          core.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        
        Serial.print("013/-/ - Current time: ");
        Serial.println(getFullDateNow());
        Serial.print("014/-/ - inside module temperature: ");
        Serial.println(getTemperature());
      }
      initialized = status;
      setAlarm();
      return status;
    };
  
    DateTime getTimeObject() {
      return core.now();
    }

    String getDateNow() {
      DateTime now = core.now();
      char text[20];
      sprintf(text, "%02d/%02d/%02d", now.day(), now.month(), now.year());
      return text;
    }

    String getTimeNow() {
      DateTime now = core.now();
      char text[20];
      sprintf(text, "%02d:%02d:%02d",  now.hour(), now.minute(), now.second());
      return text;
    }

    String getFullDateNow() {
      DateTime now = core.now();
      char text[20];
      sprintf(text, "%02d:%02d:%02d %02d/%02d/%02d",  now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
      return text;
    }

    float getTemperature() {
      return core.getTemperature();
    }

    void setAlarm(int minutes = 30) {
      alarm = DateTime(core.now().unixtime() + (20/*minutes * 60*/));
    }

    bool alarmChecker() {
      if (alarm.unixtime() < core.now().unixtime()) {
        setAlarm();
        return true;
      }
      return false;
    }
};

class Network {
  private:
    bool state = false;
    int syncWord = 0x13;
    String personalKey = "0002";
    String serverKey = "0001";
    String waitingRes;
    DateTime waitingTime;
    String networkingTableKey[20] = {};
    String networkingTableValue[20] = {};

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
        String memKey = getId();
        if (personalKey != "0000") {
          saveId(personalKey);
        } else if (personalKey == "0000" && memKey == "0000") {
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
              break;
          }
        } else if (memKey != "0000") {
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

      Serial.println("Message: " + sender + ":" + reciever + ":" + endReciever + ":" + command + ":" + msg);
      Serial.println("RSSI: " + String(LoRa.packetRssi()));
      Serial.println("Snr: " + String(LoRa.packetSnr()));
      Serial.println();

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
      } else if (endReciever.length() == 4) {
        int recieverIndex = getPathToModule(endReciever);
        String newReciever = recieverIndex > -1 ? networkingTableValue[recieverIndex] : endReciever;
        if (command == "01" && msg.length() == 4) {
          saveRoute(endReciever, newReciever);
        }
        sendMessage(msg, newReciever, endReciever, command);
      } else {
        sendMessage(
          (String&)personalKey + endReciever + command,
          sender,
          serverKey,
          (String&)"05"
        );
      }
      return false;
    };

    int getPathToModule(String id) {
      for(int i = 0; i < 20; i++) {
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
        for(int i = 0; i < 20; i++) {
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
        (String&)personalKey + endReciever + personalKey + command + message,
        getPathToModule(serverKey),
        serverKey,
        "05"
      )
    }
};

float cmpfunc (const void * a, const void * b) {
  return ( *(float*)a - *(float*)b );
}

class Sensors {
  private:
    Adafruit_BME280 bme;
    unsigned status;
    float buffer[SENSORS][SELECTION] = {{NULL}, {NULL}, {NULL}};

    float smm(float input, int sensorIndex) {
      float bufferCopy[SELECTION] = {NULL};

      for (int i = 1; i < SELECTION; i++){
        if (buffer[sensorIndex][i] == NULL) {
          buffer[sensorIndex][i]  = input;
          return input;
        }
      }

      for (int i = 0; i < SELECTION; i++) {
        if (i == 9) {
          buffer[sensorIndex][i] = input;
          bufferCopy[i] = input;
        } else {
          buffer[sensorIndex][i] = buffer[sensorIndex][i + 1];
          bufferCopy[i] = buffer[sensorIndex][i + 1];
        }
      }
      
      qsort(bufferCopy, SELECTION, sizeof(float), cmpfunc);
      float output = (bufferCopy[4] + bufferCopy[5]) / 2;
      return output;
    }
  public:
    bool init() {
      status = bme.begin(0x76);
      if (!status) {
        Serial.println("030/-/ Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        return false;
      } else {
        Serial.print("031/-/ Sensors started successfully");
        Serial.println();
        return true;
      }
    }

    Adafruit_BME280 getBME() {
      return bme;
    }

    float getTemp() {
      float input = bme.readTemperature();
      return smm(input, 0);
    }

    float getPreasure() {
      float input = bme.readPressure() / 100.0F;
      return smm(input, 1);
    }

    float getHumidity() {
      float input = bme.readHumidity();
      return smm(input, 2);
    }
};

RTC rtc;
Network network;
Sensors sensors;
bool sleep = false;
float lastSend[3]= {NULL};

void setup() {
  Serial.begin(9600);
  Serial.println("001/-/ Module started. Checking modules");
  conectionInit();
}

void loop() {
  if (sleep) {
    float batVoltage = (analogRead(A0) * (5.0/1023)* 3.2);
    if (batVoltage > 3.8) {
      network.idle();
      sleep = false;
      network.sendMessage(
        network.getPersonalKey(),
        network.getReciever(network.getServerKey()),
        network.getServerKey(),
        "04"
      );
    } else {
      delay(2UL * 60UL * 60UL * 1000UL);
    }
  } else {

    if (network.onRecieve()) {
      float input;
      switch (network.command.toInt()) {
        case 1: //Connect to module
          if (network.msg.length() == 4) {
            if(!network.saveRoute(network.msg, network.msg)) {
              network.sendError("Cant save route");
            }else {
              network.sendMessage(NULL, network.msg, network.msg, "00")
            }
          } else if (network.msg.length() == 8) {
            if(!network.saveRoute(network.msg.substring(0,4), network.msg.substring(4,8))) {
              network.sendError("Cant save route");
            }
          } else {
            network.sendError("Wrong module key");
          }
          break;
        case 2:
          getStatus();
          break;
        case 7:
          input = sensors.getTemp();
          if (checkIfCanBeSend(0, input)) {
            network.sendMessage(
              String(input),
              network.getReciever(network.getServerKey()),
              network.getServerKey(),
              String("07")
            );
            lastSend[0] = input;
          }
          break;
        case 8:
          input = sensors.getPreasure();
          if (checkIfCanBeSend(1, input)) {
            network.sendMessage(
              String(input),
              network.getReciever(network.getServerKey()),
              network.getServerKey(),
              String("08")
            );
            lastSend[1] = input;
          }
          break;
        case 9:
          input = sensors.getHumidity();
          if (checkIfCanBeSend(2, input)) {
            network.sendMessage(
              String(input),
              network.getReciever(network.getServerKey()),
              network.getServerKey(),
              String("09")
            );
            lastSend[2] = input;
          }
          break;
        default:
          Serial.println("Unknown command" + network.command);
          network.sendError("Unknown command");
          break;
      }
    }

    if (rtc.alarmChecker()) {
      float temp = sensors.getTemp();
      float preasure = sensors.getPreasure();
      float humidity = sensors.getHumidity();
      String time = rtc.getTimeNow();
      String message = String(time + "|" +
        (checkIfCanBeSend(0, temp) ? String(temp) : String("")) + "|" +
        (checkIfCanBeSend(1, preasure) ? String(preasure) : String("")) + "|" +
        (checkIfCanBeSend(2, humidity) ? String(humidity) : String(""))
      );
      Serial.println(message);
      network.sendMessage(
        message,
        network.getReciever(network.getServerKey()),
        network.getServerKey(),
        "00"
      );
    }
  } 
}

void conectionInit() {
  Serial.println("1. Connecting RTC module");
  rtc.init();
  Serial.println("2. Connecting LoRa module");
  if (rtc.initialized) {
    network.init(rtc.getTimeObject().unixtime());
  } else {
    network.init(DateTime(F(__DATE__), F(__TIME__)).unixtime());
  }
  Serial.println("3. Connecting Sensors");
  sensors.init();
}

float getStatus() {
  float batVoltage = (analogRead(A0) * (5.0/1023)* 3.2);
  if (batVoltage < 3.4) {
    network.sendMessage(
      network.getPersonalKey(),
      network.getReciever(network.getServerKey()),
      network.getServerKey(),
      String("03")
    );
    sleep = true;
    network.sleep();
  } else {
    float solVoltage = (analogRead(A1) * (5.0/1023)* 3.2);
  }
}

bool checkIfCanBeSend(int sensorId, float value) {
  if (lastSend[sensorId] == NULL) {
    return true;
  }
  float perviousValue = lastSend[sensorId];
  float diff = abs(100 - (value / (perviousValue / 100)));
  return diff > 2;
}
