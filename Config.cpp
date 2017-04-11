#include <Arduino.h>
#include "Pinner.cpp"


struct MainConfig {
    uint16_t noActionAfterStartWithinSec;
    uint8_t sensorsNumber;
    uint8_t sensorOffNotConnectedMicros;
    uint32_t sensorOnMillis;
    uint32_t sensorOffMaxWaitMillis;
    char tapOpen[20];
    char tapClose[20];
    char tapStop[20];
    uint16_t tapActionSec;
    uint8_t buzzerPin;
    char buzzerHz[100];
    char buzzerDurationMillis[100];
    char blynkAuthToken[33];
    char blynkHost[100];
    uint16_t blynkPort;
    char blynkSSLFingerprint[60];
    uint8_t vPinAlarmStatus;
    uint8_t vPinStatus;
    uint8_t vPinAlarmForce;
    uint8_t vPinAlarmMute;
    uint8_t vPinTapOpen;
    uint8_t vPinTapClose;
    uint8_t vPinTapStop;
};


struct SensorConfig {
    bool active;
    uint8_t pin;
    uint8_t vPin;
    uint32_t microsWhenAlarm;
    char name[100];
};


class Config {
public:
    MainConfig main;
    SensorConfig **sensors = 0;
    Pinner pinner;

private:
    bool sensorsCreated = false;

public:
// 0 - MainConfig
// 1 - sensor 1 MainConfig = MainConfig
// 2 - sensor 2 MainConfig = MainConfig + sensor 1 SensorConfig
// 3 - sensor 3 MainConfig = MainConfig + sensor 1 SensorConfig + sensor 2 SensorConfig
    uint32_t getEEPROMConfigAddress(uint8_t confNumber) {
        uint32_t address = 0;

        if (confNumber >= 1)
            address += sizeof(MainConfig);

        if (confNumber >= 2)
            address += (confNumber - 1) * sizeof(SensorConfig);

        return address;
    }


    uint32_t getEEPROMConfigTotalSize() {
        return getEEPROMConfigAddress(main.sensorsNumber + 1);
    }


    void insertSensorConfig(uint8_t sensorNumber, uint8_t sensorsTotal) {
        uint8_t i;

        SensorConfig **copy = new SensorConfig *[sensorsTotal + 1];

        for (i = 0; i < sensorNumber; i++)
            copy[i] = sensors[i];

        copy[sensorNumber] = new SensorConfig;

        for (i = sensorNumber; i < sensorsTotal; i++)
            copy[i + 1] = sensors[i];

        if (sensorsCreated)
            delete[] sensors;
        else
            sensorsCreated = true;

        sensors = copy;
    }


    void addSensorConfig(uint8_t sensorsTotal = 255) {
        if (sensorsTotal == 255)
            insertSensorConfig(main.sensorsNumber, main.sensorsNumber);
        else
            insertSensorConfig(sensorsTotal, sensorsTotal);
    }


    void deleteSensorConfig(uint8_t sensorNumber, uint8_t sensorsTotal) {
        uint8_t i;

        SensorConfig **copy = new SensorConfig *[sensorsTotal - 1];

        for (i = 0; i < sensorNumber; i++)
            copy[i] = sensors[i];

        for (i = sensorNumber + 1; i < sensorsTotal; i++)
            copy[i - 1] = sensors[i];

        delete[] sensors[sensorNumber];
        delete[] sensors;

        sensors = copy;
    }


    bool fixString(char *str, uint32_t strLen) {
        bool fixed = false;
        char replaceTo = '_';

        if (strlen(str) > strLen) {
            str[0] = 0;
            fixed = true;
        } else
            for (uint32_t i = 0; i < strlen(str); i++)
                if ((str[i] < 0x20) || (str[i] == 0xFF)) {
                    str[i] = replaceTo;
                    fixed = true;
                }

        return fixed;
    }


    void fixStringAndSetFlag(char *str, uint32_t strLen, bool &flag) {
        flag = fixString(str, strLen);
    }


    bool fixMainConfig() {
        bool fixed = false;

        if (main.sensorsNumber > 10) {
            main.sensorsNumber = 10;
            fixed = true;
        }

        if (main.sensorOnMillis > 10000) {
            main.sensorOnMillis = 10000;
            fixed = true;
        }

        if (main.sensorOffMaxWaitMillis > 5000) {
            main.sensorOffMaxWaitMillis = 5000;
            fixed = true;
        }

        fixStringAndSetFlag(main.buzzerHz, sizeof(main.buzzerHz), fixed);
        fixStringAndSetFlag(main.buzzerDurationMillis, sizeof(main.buzzerDurationMillis), fixed);
        fixStringAndSetFlag(main.blynkAuthToken, sizeof(main.blynkAuthToken), fixed);
        fixStringAndSetFlag(main.blynkHost, sizeof(main.blynkHost), fixed);
        fixStringAndSetFlag(main.blynkSSLFingerprint, sizeof(main.blynkSSLFingerprint), fixed);
        fixStringAndSetFlag(main.tapOpen, sizeof(main.tapOpen), fixed);
        fixStringAndSetFlag(main.tapClose, sizeof(main.tapClose), fixed);
        fixStringAndSetFlag(main.tapStop, sizeof(main.tapStop), fixed);

        return fixed;
    }


    bool fixSensorConfig(uint8_t sensor) {
        bool fixed = false;

        if (fixString(sensors[sensor]->name, sizeof(sensors[sensor]->name)))
            fixed = true;

        if (pinner.fixPinAndSetStatus(sensors[sensor]->pin, sensors[sensor]->active))
            fixed = true;

        return fixed;
    }


    void returnMainConfigStr(char *str, uint32_t strLen) {
        uint32_t sLen;

        snprintf(str, strLen,
                 "No Action After Start Within Seconds=%u\nSensors Number=%u\nNot Connected Value=%u\nSensors On Time (millis)=%lu\nSensors Off Max Wait Time (millis)=%lu\nBuzzer Pin=%u\nBuzzer Hz: %s\nBuzzer Durations (millis): %s",
                 main.noActionAfterStartWithinSec,
                 main.sensorsNumber,
                 main.sensorOffNotConnectedMicros,
                 main.sensorOnMillis,
                 main.sensorOffMaxWaitMillis,
                 main.buzzerPin,
                 main.buzzerHz,
                 main.buzzerDurationMillis);
    }


    void returnSensorConfigStr(char *str, uint32_t strLen, uint8_t sensor) {
        snprintf(str, strLen,
                 "%u) Sensor Pin=%03u, Alarm Value=%010lu, Name=%s",
                 sensor,
                 sensors[sensor]->pin,
                 sensors[sensor]->microsWhenAlarm,
                 sensors[sensor]->name);
    }
};
