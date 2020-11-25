#include "Arduino.h"
#include <Wire.h>

#define SELECTION_SIZE 5
#define SENSORS_AMOUNT 3

class Sensors {
  private:
    static const uint16_t dig_T1 = (int16_t)28422;
    static const int16_t dig_T2 = (int16_t)26531;
    static const int16_t dig_T3 = (int16_t)50;

    static const uint16_t dig_P1 = (int16_t)37497;
    static const int16_t dig_P2 = (int16_t)-10600;
    static const int16_t dig_P3 = (int16_t)3024;
    static const int16_t dig_P4 = (int16_t)8793;
    static const int16_t dig_P5 = (int16_t)-108;
    static const int16_t dig_P6 = (int16_t)-7;
    static const int16_t dig_P7 = (int16_t)9900;
    static const int16_t dig_P8 = (int16_t)-10230;
    static const int16_t dig_P9 = (int16_t)4285;

    static const uint8_t dig_H1 = (uint8_t)75;
    static const int16_t dig_H2 = (int16_t)383;
    static const uint8_t dig_H3 = (uint8_t)0;
    static const int16_t dig_H4 = (int16_t)272;
    static const int16_t dig_H5 = (int16_t)50;
    static const int8_t dig_H6 = (uint8_t)30;

    static const uint8_t _i2caddr = 0x76;
    int32_t t_fine;
    
    unsigned status;
    uint8_t m_dig[32];

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

    uint32_t read24(byte reg) {
      uint32_t value;

      Wire.beginTransmission((uint8_t)_i2caddr);
      Wire.write((uint8_t)reg);
      Wire.endTransmission();
      Wire.requestFrom((uint8_t)_i2caddr, (byte)3);

      value = Wire.read();
      value <<= 8;
      value |= Wire.read();
      value <<= 8;
      value |= Wire.read();

      return value;
    }

    float readTemperature(void) {
      int32_t var1, var2;

      int32_t adc_T = read24(0xFA);
      if (adc_T == 0x800000) // value in case temp measurement was disabled
        return NAN;
      adc_T >>= 4;

      var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) *
              ((int32_t)dig_T2)) >>
             11;

      var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
                ((adc_T >> 4) - ((int32_t)dig_T1))) >>
               12) *
              ((int32_t)dig_T3)) >>
             14;

      t_fine = var1 + var2 + 0;

      float T = (t_fine * 5 + 128) >> 8;
      return T / 100;
    }

    uint16_t read16(byte reg) {
      uint16_t value;
      Wire.beginTransmission((uint8_t)_i2caddr);
      Wire.write((uint8_t)reg);
      Wire.endTransmission();
      Wire.requestFrom((uint8_t)_i2caddr, (byte)2);
      value = (Wire.read() << 8) | Wire.read();
      return value;
    }

    float readPressure(void) {
      int64_t var1, var2, p;

      readTemperature(); // must be done first to get t_fine

      int32_t adc_P = read24(0xF7);
      if (adc_P == 0x800000) // value in case pressure measurement was disabled
        return NAN;
      adc_P >>= 4;

      var1 = ((int64_t)t_fine) - 128000;
      var2 = var1 * var1 * (int64_t)dig_P6;
      var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
      var2 = var2 + (((int64_t)dig_P4) << 35);
      var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
             ((var1 * (int64_t)dig_P2) << 12);
      var1 =
          (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

      if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
      }
      p = 1048576 - adc_P;
      p = (((p << 31) - var2) * 3125) / var1;
      var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
      var2 = (((int64_t)dig_P8) * p) >> 19;

      p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);

      return ((float)p / 256) / 100.0F;
    }

    float readHumidity(void) {
      readTemperature(); // must be done first to get t_fine

      int32_t adc_H = read16(0xFD);
      if (adc_H == 0x8000) // value in case humidity measurement was disabled
        return NAN;

      int32_t v_x1_u32r;

      v_x1_u32r = (t_fine - ((int32_t)76800));

      v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) -
                      (((int32_t)dig_H5) * v_x1_u32r)) +
                     ((int32_t)16384)) >>
                    15) *
                   (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
                        (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) +
                         ((int32_t)32768))) >>
                       10) +
                      ((int32_t)2097152)) *
                         ((int32_t)dig_H2) +
                     8192) >>
                    14));

      v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                                 ((int32_t)dig_H1)) >>
                                4));

      v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
      v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
      float h = (v_x1_u32r >> 12);
      return h / 1024.0;
    }

  public:
    bool init() {
      // Wire.begin();
      Wire.beginTransmission(_i2caddr);
      Wire.write((uint8_t)0xD0);
      Wire.endTransmission();
      Wire.requestFrom(_i2caddr,(byte)1);
      status = Wire.read();
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
      return smm(readTemperature(), 0);
    }

    float getPreasure() {
      return smm(readPressure(), 1);
    }

    float getHumidity() {
      return smm(readHumidity(), 2);
    }
};
