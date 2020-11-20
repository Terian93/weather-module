#include "Arduino.h"
#include "RTClib.h"

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