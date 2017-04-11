#include <Arduino.h>


enum SensorStatus {
    OFF,
    GAP,
    LEAK,
    GOOD
};


struct SensorData {
    bool processed;
    uint32_t offMicros;
    SensorStatus status;
};


class SensorsData {
public:
    SensorData *sensor = 0;

private:
    bool created = false;
    uint8_t sensorsNumber = 0;

public:
    void setSensorsNumber(uint8_t sNumber) {
        if (sNumber >= 1) {
            SensorData *temp = new SensorData[sNumber];

            if (created) {
                delete[] sensor;
            } else
                created = true;

            sensor = temp;
            sensorsNumber = sNumber;
        }
    }

    uint8_t getSensorsNumber() {
        return sensorsNumber;
    }
};
