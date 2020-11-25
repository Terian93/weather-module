#include "rtc.cpp"
#include "network.cpp"
#include "sensors.cpp"
#include <Wire.h>
#include <LoRa.h>
#include <limits.h>

#define ANALOG_READ_COEF 0.0156402737047898 //(5.0 / 1023)* 3.2

static const String DIVIDER = String("|");

RTC rtc;
Network network;
Sensors sensors;
bool sleep = false;
float lastSend[SENSORS_AMOUNT]= {NULL};

float solVoltage;
float insideTemp;
float batVoltage;

float temp;
float preasure;
float humidity;
String timestring;
String datamessage;

void checkStatus(bool canSendStatus = false) {
  batVoltage = (analogRead(0) * ANALOG_READ_COEF);
  solVoltage = (analogRead(1) * ANALOG_READ_COEF);
  insideTemp = rtc.getTemperature();
  if (batVoltage < 3.4) {
    network.sendMessage(
      network.getPersonalKey(),
      network.getReciever(network.getServerKey()),
      network.getServerKey(),
      String("03")
    );
    sleep = true;
    network.sleep();
  } else if (canSendStatus) {
    network.sendMessage(
      (String) rtc.getFullDateNow() + DIVIDER + batVoltage + DIVIDER + solVoltage + DIVIDER + insideTemp,//TODO: Thats not working. Change with concat or other method
      network.getReciever(network.getServerKey()),
      network.getServerKey(),
      String("02")
    );
  }
}

bool checkIfCanBeSend(int sensorId, float value) {
  if (lastSend[sensorId] == NULL) {
    lastSend[sensorId] = value;
    return true;
  }
  float diff = abs(100 - (value / (lastSend[sensorId] / 100)));
  if (diff > 2) {
    lastSend[sensorId] = value;
    return true;
  } else {
    return false;
  };
}

void setup() {
  Serial.begin(9600);
  Serial.println("001/-/ Module started. Checking modules");
  Serial.println("1. Connecting RTC module");
  rtc.init();
  Serial.println("2. Connecting LoRa module");
  if (rtc.initialized) {
    network.init(rtc.getTimeObject().unixtime());
  } else {
    network.init(DateTime(__DATE__, __TIME__).unixtime());
  }
  Serial.println("3. Connecting Sensors");
  sensors.init();
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
      delay(6UL * 60UL * 60UL * 1000UL);
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
              network.sendMessage((String)"", network.msg, network.msg, "00");
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
          checkStatus(true);
          break;
//        case 7:
//          input = sensors.getTemp();
//          if (checkIfCanBeSend(0, input)) {
//            network.sendMessage(
//              String(input),
//              network.getReciever(network.getServerKey()),
//              network.getServerKey(),
//              String("07")
//            );
//            lastSend[0] = input;
//          }
//          break;
//        case 8:
//          input = sensors.getPreasure();
//          if (checkIfCanBeSend(1, input)) {
//            network.sendMessage(
//              String(input),
//              network.getReciever(network.getServerKey()),
//              network.getServerKey(),
//              String("08")
//            );
//            lastSend[1] = input;
//          }
//          break;
//        case 9:
//          input = sensors.getHumidity();
//          if (checkIfCanBeSend(2, input)) {
//            network.sendMessage(
//              String(input),
//              network.getReciever(network.getServerKey()),
//              network.getServerKey(),
//              String("09")
//            );
//            lastSend[2] = input;
//          }
//          break;
        default:
          Serial.println("Unknown command" + network.command);
          network.sendError("Unknown command");
          break;
      }
    }
    if (rtc.alarmChecker()) {
      temp = sensors.getTemp();
      preasure = sensors.getPreasure();
      humidity = sensors.getHumidity();
      timestring = String(rtc.getFullDateNow());
      Serial.println("sent");
      LoRa.beginPacket();
      LoRa.print(network.getPersonalKey());
      LoRa.print(network.getReciever(network.getServerKey()));
      LoRa.print(network.getServerKey());
      LoRa.print("00");
      LoRa.print(timestring);
      LoRa.print(DIVIDER);
      LoRa.print(checkIfCanBeSend(0, temp) ? String(temp) : String(""));
      LoRa.print(DIVIDER);
      LoRa.print(checkIfCanBeSend(1, preasure) ? String(preasure) : String(""));
      LoRa.print(DIVIDER);
      LoRa.print(checkIfCanBeSend(2, humidity) ? String(humidity) : String(""));
      LoRa.endPacket();
      delay(2000);
      checkStatus(false);
    }
  } 
}
