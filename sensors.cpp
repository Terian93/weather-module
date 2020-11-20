#include "Arduino.h"
#include <Wire.h>
#include <BME280I2C.h>

#define SELECTION_SIZE 5
#define SENSORS_AMOUNT 3

class Sensors {
  private:
    BME280I2C bme;
    unsigned status;
    float buffer[SENSORS_AMOUNT][SELECTION_SIZE] = {{NULL}, {NULL}, {NULL}};

    static float cmpfunc (const void * a, const void * b) {
      return ( *(float*)a - *(float*)b );
    }

    float smm(float input, int sensorIndex) {
      float bufferCopy[SELECTION_SIZE] = {NULL};

      for (int i = 1; i < SELECTION_SIZE; i++){
        if (buffer[sensorIndex][i] == NULL) {
          buffer[sensorIndex][i]  = input;
          return input;
        }
      }

      for (int i = 0; i < SELECTION_SIZE; i++) {
        if (i == 9) {
          buffer[sensorIndex][i] = input;
          bufferCopy[i] = input;
        } else {
          buffer[sensorIndex][i] = buffer[sensorIndex][i + 1];
          bufferCopy[i] = buffer[sensorIndex][i + 1];
        }
      }
      
      qsort(bufferCopy, SELECTION_SIZE, sizeof(float), cmpfunc);
      float output = (bufferCopy[4] + bufferCopy[5]) / 2;
      return output;
    }
  public:
    bool init() {
      Wire.begin();
      status = bme.begin();
      if (!status) {
        Serial.println("030/-/ Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        return false;
      } else {
        Serial.print("031/-/ Sensors started successfully");
        Serial.println();
        return true;
      }
    }

    float getTemp() {
      float input = bme.temp();
      return smm(input, 0);
    }

    float getPreasure() {
      float input = bme.pres();
      return smm(input, 1);
    }

    float getHumidity() {
      float input = bme.hum();
      return smm(input, 2);
    }
};
