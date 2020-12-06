#include <Wire.h>
#include <Arduino.h>
#include "RTClib.h"

class RTC {
  private:
    RTC_DS3231 core;
    long alarm = __FLT_MAX__;
    DateTime nowTime;
    bool status;
  public:
    bool initialized = false;
    bool init() {
      status = core.begin();    
      if (!status) {
        Serial.println("010/-/ - Starting RTC failed!");
      } else {
        Serial.println("011/-/ - RTC started successfully");
        if (core.lostPower()) {
          Serial.println("012/-/ RTC lost power, please adjust time");
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
      char text[20];
      nowTime = core.now();
      sprintf(text, "%04d-%02d-%02dT%02d:%02d:%02d", nowTime.year(), nowTime.month(), nowTime.day(), nowTime.hour(), nowTime.minute(), nowTime.second());
      return String(text);
    }

    float getTemperature() {
      return core.getTemperature();
    }

    void setAlarm(int minutes = 30) {
      alarm = millis() + (5 * 1000 /*minutes * 60 * 1000*/);
    }

    bool alarmChecker() {
      if (millis() >= alarm) {
        // Serial.println(alarm.unixtime() < core.now().unixtime());
        setAlarm();
        return true;
      }
      return false;
    }
};
