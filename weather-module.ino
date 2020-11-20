#include "rtc.cpp"
#include "network.cpp"
#include "sensors.cpp"

#define ANALOG_READ_COEF 0.0156402737047898 //(5.0 / 1023)* 3.2
#define DIVIDER '|'

RTC rtc;
Network network;
Sensors sensors;
bool sleep = false;
float lastSend[3]= {NULL};

float solVoltage;
float insideTemp;
float batVoltage;

void conectionInit() {
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

void checkStatus(bool canSendStatus = false) {
  batVoltage = (analogRead(A0) * ANALOG_READ_COEF);
  solVoltage = (analogRead(A1) * ANALOG_READ_COEF);
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
      (String) rtc.getFullDateNow() + DIVIDER + batVoltage + DIVIDER + solVoltage + DIVIDER + insideTemp,
      network.getReciever(network.getServerKey()),
      network.getServerKey(),
      String("02")
    );
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
          checkStatus(true);
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
      String time = rtc.getFullDateNow();
      String message = String(time + DIVIDER +
        (checkIfCanBeSend(0, temp) ? String(temp) : String("")) + DIVIDER +
        (checkIfCanBeSend(1, preasure) ? String(preasure) : String("")) + DIVIDER +
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
