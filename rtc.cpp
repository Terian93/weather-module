#include <Wire.h>
#include <Arduino.h>
#include "RTClib.h"

#define DS3231_ADDRESS 0x68

class RTC {
  private:
    RTC_DS3231 core;
    DateTime alarm;
    DateTime nowTime;
    char text[20];
    bool status;
  public:
    bool initialized = false;
    bool init() {
      Wire.beginTransmission(DS3231_ADDRESS);    
      if (Wire.endTransmission() == 0) {
        Serial.println("010/-/ - Starting RTC failed!");
      } else {
        Serial.println("011/-/ - RTC started successfully");
        if (core.lostPower()) {
          Serial.println("012/-/ RTC lost power, let's set the time!");
          core.adjust(DateTime(__DATE__, __TIME__));
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

    String getFullDateNow() {
      nowTime = core.now();
      sprintf(text, "%02d:%02d:%02d %02d/%02d/%02d",  nowTime.hour(), nowTime.minute(), nowTime.second(), nowTime.day(), nowTime.month(), nowTime.year());
      return String(text);
    }

    float getTemperature() {
      return core.getTemperature();
    }

    void setAlarm(int minutes = 30) {
      alarm = DateTime(core.now().unixtime() + (3/*minutes * 60*/));
    //  sprintf(text, "%02d:%02d:%02d %02d/%02d/%02d",  alarm.hour(), alarm.minute(), alarm.second(), alarm.day(), alarm.month(), alarm.year());
    //  Serial.println(text);
    //  now = core.now();
    //  sprintf(text, "%02d:%02d:%02d %02d/%02d/%02d",  now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
    //  Serial.println(text);
    }

    bool alarmChecker() {
      if (alarm.unixtime() < core.now().unixtime()) {
        // Serial.println(alarm.unixtime() < core.now().unixtime());
        setAlarm();
        return true;
      }
      return false;
    }
};
