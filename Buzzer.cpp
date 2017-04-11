#include <Arduino.h>


class Buzzer {
private:
    bool buzzerCreated = false;

public:
    bool buzzerFlag;
    uint8_t pin;
    uint16_t *buzzerHz;
    uint16_t *buzzerDurationMillis;
    uint32_t buzzerNotes;
    uint32_t curNote;
    uint32_t lastBuzzerMillis;


    void init(uint32_t notes) {
        if (buzzerCreated) {
            delete[] buzzerHz;
            delete[] buzzerDurationMillis;
        } else
            buzzerCreated = true;

        curNote = 0;
        buzzerNotes = notes;
        buzzerHz = new uint16_t[buzzerNotes];
        buzzerDurationMillis = new uint16_t[buzzerNotes];
        pinMode(pin, OUTPUT);
    }


    void init() {
        init(buzzerNotes);
    }


    void beep(bool doBeep = true) {
        if (doBeep) {
            if ((buzzerHz[curNote] > 0) && (buzzerDurationMillis[curNote] > 0))
                tone(pin, buzzerHz[curNote], buzzerDurationMillis[curNote]);

            if (curNote >= buzzerNotes - 1)
                curNote = 0;
            else
                curNote++;

            buzzerFlag = true;
        } else {
            noTone(pin);
            buzzerFlag = false;
        }

        lastBuzzerMillis = millis();
    }
};
