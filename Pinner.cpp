#include <Arduino.h>


class Pinner {
public:
#ifdef ESP8266
    const uint8_t allowedPins[11] = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16};
#else
    const uint8_t allowedPins[1] = {0};
#endif
    const uint8_t allowedPinsLength = sizeof(allowedPins) / sizeof(allowedPins[0]);
    const uint8_t ignoredPin = 255;


    bool isAllowedPin(uint8_t pin) {
        bool allowedPin = false;

        for (uint8_t i = 0; i < allowedPinsLength; i++)
            if (pin == allowedPins[i]) {
                allowedPin = true;
                break;
            }

        return allowedPin;
    }


    bool fixPin(uint8_t &pin) {
        bool fixed;

        if (isAllowedPin(pin)) {
            fixed = false;
        } else {
            pin = ignoredPin;
            fixed = true;
        }

        return fixed;
    }


    bool fixPinAndSetStatus(uint8_t &pin, bool &status) {
        bool fixed = fixPin(pin);

        if (fixed)
            status = false;

        return fixed;
    }
};
