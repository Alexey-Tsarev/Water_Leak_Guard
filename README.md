# Water Leak Guard
This is an ESP8266 project intended to protect you against water leaks.

## Components
It consists from components:
 - several sensors
 - buzzer
 - valve manipulator

## Features
The project features:
 - Detects water leak and close tap
 - Alarm
 - Notifications (via Blynk) to a smartphone
 - GUI for smartphone (via Blynk)
 - Web GUI
 - WiFi configure (via WiFiManager: https://github.com/tzapu/WiFiManager)

## Schema
Project schema is here:  
https://github.com/AlexeySofree/Water_Leak_Guard/issues/1

## Algorithm
Logic of project is pretty simple.  
A sensor is a capacitor, outlying to some distance via two wires.  
A microcontroller charges a sensor's capacitor for sometimes (by setting logic 1 to according pin).  
Then the microcontroller switches the pin to INPUT and measures time while a value on pin == 1.  
When value on pin == 0 this means that the capacitor discharged.
If a calculated time (or some other events) is less then some threshold,
then it means that something wrong happened and the algorithm does the following alarm actions:
 - Close tap
 - Alarm by buzzer (melody is configurable via Web UI)
 - Send a notification to smartphone (via Blynk)

## Web UI
The project has a Web UI. Features / Settings:
 - Login / Password protection
 - Pure HTML / JavaScript. No internet connection required
 - Status
 - Sensors number
 - Sensors data
 - Blynk parameters (there is a way to use own Blynk server)
 - Virtual pins settings (for Blynk interaction)
 - Real ESP8266 pins settings (sensors, buzzer, relays)
 - Tap actions (pins values when an appropriate tap event has the active status)
 - Alarm melody (Frequencies and Durations)
 - Multilanguage (not implemented)

## Smartphone UI
Based on Blynk. Features / Settings:
 - Close, open, stop tap
 - Force alarm (just for checking that buzzer is working)
 - Mute alarm
 - Turn on / off any sensor
 - Sensors status as a json log

## Sensors pins
Not all ESP8266 pins are ready to work with sensors (because some of them have pull resistors). Please, consider this.  
Working pins are:
~~~
16 - D0
 5 - D1
 4 - D2
14 - D5
12 - D6
13 - D7
~~~

## Images
Images are here:  
https://github.com/AlexeySofree/Water_Leak_Guard/issues/2

---

Good Luck and Best Regards,  
Alexey Tsarev, Tsarev.Alexey at gmail.com
