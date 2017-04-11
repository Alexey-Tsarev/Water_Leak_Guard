#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "Water_Leak_Guard.h"
#include "Config.cpp"
#include "EEPROMer.h"
#include "Sensors.cpp"
#include "Uptimer.cpp"
#include "Elapser.cpp"
#include "Buzzer.cpp"
#include "Index.h"


// Config Blynk
//#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG
//#define configBlinkSSL
#ifdef configBlinkSSL

#include <BlynkSimpleEsp8266_SSL.h>

#else

#include <BlynkSimpleEsp8266.h>

#endif
// End Config Blynk

#define configSerialSpeed 115200

#define configWiFiManagerIsActive
#define configWiFiManagerAPName               "WaterLeakGuard"
#define configWiFiManagerConfigTimeoutSeconds 300

#define configHTTPPort 80
#define configHTTPUsername configWiFiManagerAPName
#define configHTTPPassword String(ESP.getChipId()).c_str()

#define configOTAisActive
#define configOTAPassword String(ESP.getChipId()).c_str()

#define cronTasksEverySec 15

bool WiFiConnectedFlag, inActionFlag, beepFlag, alarmFlag, alarmForcedFlag, alarmMuteFlag, blynkFlag, blynkSSLFlag, tapInActionFlag;
uint8_t processedSensorsNumber = 0;
char strBuf[2048], strBuf2[2048], sensorsStatus[2048];
uint32_t i, j, lastCronTaskSec, lastSensorsOnMicros, lastSensorsOffMicros, sensorOnMicros, sensorOffMaxWaitMicros, lastTapActionSec;
ESP8266WebServer server(configHTTPPort);
Config conf;
SensorsData sensors;
Uptimer uptimer;
Buzzer buzzer;
uint32_t Elapser::lastTime;


void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case WIFI_EVENT_STAMODE_GOT_IP:
            if (!WiFiConnectedFlag) {
                WiFiConnectedFlag = true;

                log("WiFi is ON");
                lg("Connected to: ");
                lg(WiFi.SSID().c_str());
                lg(" / ");
                log(WiFi.localIP().toString().c_str());
            }

            break;
        case WIFI_EVENT_STAMODE_DISCONNECTED:
            if (WiFiConnectedFlag) {
                WiFiConnectedFlag = false;

                log("WiFi is OFF");
            }

            break;
    }
}


void printConfigs() {
    conf.returnMainConfigStr(strBuf, sizeof(strBuf));
    log(strBuf);

    for (i = 0; i < conf.main.sensorsNumber; i++) {
        conf.returnSensorConfigStr(strBuf, sizeof(strBuf), i);
        log(strBuf);
    }

    log();
}


void APConfigCallback(WiFiManager *myWiFiManager) {
}


uint8_t getCommasNumber(char *str) {
    uint8_t commas = 0;

    for (uint32_t i = 0; i < strlen(str); i++)
        if (str[i] == ',')
            commas++;

    return commas;
}


void tapAction(char *str) {
    bool pinSet = false, valSet = false;
    char tmpStr[10];
    uint8_t pin = 0, val = 0, strLen = strlen(str), curTmpStrPos = 0;

    lg("Tap Action. Setting pin(s): ");
    log(str);

    for (uint8_t i = 0; i < strLen; i++) {
        if (curTmpStrPos < sizeof(tmpStr))
            tmpStr[curTmpStrPos] = str[i];

        if (str[i] == '=') {
            tmpStr[curTmpStrPos] = 0;
            pin = (uint8_t) atoi(tmpStr);
            pinSet = true;
            curTmpStrPos = 0;
        } else if ((str[i] == ',') || (i == strLen - 1)) {
            if (i == strLen - 1)
                curTmpStrPos++;

            tmpStr[curTmpStrPos] = 0;
            val = (uint8_t) atoi(tmpStr);
            valSet = true;
            curTmpStrPos = 0;
        } else
            curTmpStrPos++;

        if (pinSet && valSet) {
            snprintf(strBuf, sizeof(strBuf), "Setting the pin %u to the %u", pin, val);
            lg(strBuf);

            if (conf.pinner.isAllowedPin(pin)) {
                pinMode(pin, OUTPUT);

                if (val >= 1)
                    digitalWrite(pin, HIGH);
                else
                    digitalWrite(pin, LOW);

                log();
            } else {
                log(" - IGNORED");
            }

            pinSet = false;
            valSet = false;
        }
    }

    log();
}


void startTapActionAndSetFlag(char *str, bool &b) {
    tapAction(str);
    b = true;
    lastTapActionSec = millis() / 1000;
}


void startTapActionAndUnsetFlag(char *str, bool &b) {
    tapAction(str);
    b = false;
}


void blynkWriteTerminal(char *str) {
    if (blynkFlag) {
        WidgetTerminal terminal(conf.main.vPinStatus);
        terminal.println(str);
        terminal.flush();
    }
}


void blynkNotify(char *str) {
    if (blynkFlag)
        Blynk.notify(str);
}


void blynkWriteTerminalAndNotify(char *str) {
    blynkWriteTerminal(str);
    blynkNotify(str);
}


void blynkWriteAlarmStatus() {
    if (blynkFlag && alarmFlag)
        Blynk.virtualWrite(conf.main.vPinAlarmStatus, "ALERT");
    else
        Blynk.virtualWrite(conf.main.vPinAlarmStatus, "GOOD");
}


BLYNK_CONNECTED() {
    log("Connected to Blynk");

    Blynk.virtualWrite(conf.main.vPinAlarmForce, alarmForcedFlag);
    Blynk.virtualWrite(conf.main.vPinAlarmMute, alarmMuteFlag);
    blynkWriteAlarmStatus();

    for (uint8_t i = 0; i < conf.main.sensorsNumber; i++)
        Blynk.virtualWrite(conf.sensors[i]->vPin, conf.sensors[i]->active);
}


BLYNK_WRITE_DEFAULT() {
    uint8_t pin = request.pin;
    strcopy(strBuf2, param.asStr());
    snprintf(strBuf, sizeof(strBuf), "Got Blynk write - vPin: %u, val: %s", pin, strBuf2);
    log(strBuf);
    blynkWriteTerminal(strBuf);

    if (pin == conf.main.vPinAlarmForce)
        alarmForcedFlag = param.asInt();

    else if (pin == conf.main.vPinAlarmMute)
        alarmMuteFlag = param.asInt();

    else if ((pin == conf.main.vPinTapOpen) && (param.asInt() == 1))
        openTap();

    else if ((pin == conf.main.vPinTapClose) && (param.asInt() == 1))
        closeTap();

    else if ((pin == conf.main.vPinTapStop) && (param.asInt() == 1))
        stopTap();

    else {
        for (uint8_t i = 0; i < conf.main.sensorsNumber; i++) {
            if (pin == conf.sensors[i]->vPin) {
                conf.sensors[i]->active = param.asInt();

                if (conf.fixSensorConfig(i))
                    Blynk.virtualWrite(pin, conf.sensors[i]->active);

                EEPROMUpdater(*conf.sensors[i], 0, conf.getEEPROMConfigAddress(i + 1),
                              conf.getEEPROMConfigTotalSize());
                break;
            }
        }
    }
}


BLYNK_READ_DEFAULT() {
    uint8_t pin = request.pin;
    snprintf(strBuf, sizeof(strBuf), "Got blynk read, pin: %u", pin);
    log(strBuf);

    if (pin == conf.main.vPinAlarmStatus)
        blynkWriteAlarmStatus();
}


void blynkVirtualWriteBool(uint8_t vPin, bool val, bool &val1) {
    Blynk.virtualWrite(vPin, val);
    val1 = val;
}


void openTap() {
    startTapActionAndSetFlag(conf.main.tapOpen, tapInActionFlag);
    strcopy(strBuf, "[OPEN TAP]");
    log(strBuf);
    blynkWriteTerminal(strBuf);
}


void closeTap() {
    startTapActionAndSetFlag(conf.main.tapClose, tapInActionFlag);
    strcopy(strBuf, "[CLOSE TAP]");
    log(strBuf);
    blynkWriteTerminal(strBuf);
}


void stopTap () {
    startTapActionAndUnsetFlag(conf.main.tapStop, tapInActionFlag);
    strcopy(strBuf, "[STOP TAP]");
    log(strBuf);
    blynkWriteTerminal(strBuf);
}


// 0 - all
// 1 - main
// 2 - sensors
void loadConfigs(uint8_t cfg = 0, bool buz = true, bool bl = true) {
    // Load Main Config
    if (cfg <= 1) {
        EEPROMReader(conf.main, 0, conf.getEEPROMConfigAddress(0));
        conf.fixMainConfig();
        stopTap();
    }
    // End Load Main Config

    // Load Sensors Config
    if (cfg <= 2) {
        if (conf.main.sensorsNumber != sensors.getSensorsNumber())
            sensors.setSensorsNumber(conf.main.sensorsNumber);

        for (i = 0; i < conf.main.sensorsNumber; i++) {
            conf.addSensorConfig(i);
            EEPROMReader(*conf.sensors[i], 0, conf.getEEPROMConfigAddress(i + 1));
            conf.fixSensorConfig(i);
        }
    }
    //  End Load Sensors Config

    // Buzzer
    if (buz) {
        // Calculate buzzer array elements number and init
        buzzer.buzzerNotes = 1 + _min(getCommasNumber(conf.main.buzzerHz),
                                      getCommasNumber(conf.main.buzzerDurationMillis));
        buzzer.pin = conf.main.buzzerPin;
        buzzer.init();
        // End

        // Parsing string with comma separated numbers and put them to arrays
        returnUNumArrayFromCommaSeparatedStr(buzzer.buzzerHz, buzzer.buzzerNotes,
                                             conf.main.buzzerHz);
        returnUNumArrayFromCommaSeparatedStr(buzzer.buzzerDurationMillis, buzzer.buzzerNotes,
                                             conf.main.buzzerDurationMillis);
        // End

        // Put data back to string (to fix not valid data (non-numeric))
        returnCSVStrFromUNumArray(conf.main.buzzerHz, sizeof(conf.main.buzzerHz),
                                  buzzer.buzzerHz, buzzer.buzzerNotes, false);
        returnCSVStrFromUNumArray(conf.main.buzzerDurationMillis, sizeof(conf.main.buzzerDurationMillis),
                                  buzzer.buzzerDurationMillis, buzzer.buzzerNotes, false);
        // End
    }
    // End Buzzer

    // Blynk
    if (bl) {
        if (Blynk.connected())
            Blynk.disconnect();

        blynkFlag = strlen(conf.main.blynkAuthToken) > 0;

        if (blynkFlag) {
            blynkSSLFlag = strlen(conf.main.blynkSSLFingerprint) > 0;
#ifdef configBlinkSSL
            if (blynkSSLFlag)
                Blynk.config(conf.main.blynkAuthToken, conf.main.blynkHost, conf.main.blynkPort,
                             conf.main.blynkSSLFingerprint);
#endif
            if (!blynkSSLFlag)
                if (strlen(conf.main.blynkHost) > 0)
                    if (conf.main.blynkPort > 0)
                        Blynk.config(conf.main.blynkAuthToken, conf.main.blynkHost, conf.main.blynkPort);
                    else
                        Blynk.config(conf.main.blynkAuthToken, conf.main.blynkHost);
                else
                    Blynk.config(conf.main.blynkAuthToken);

            Blynk.connect(2000);
        }
    }
    // End Blynk

    sensorOnMicros = conf.main.sensorOnMillis * 1000;
    sensorOffMaxWaitMicros = conf.main.sensorOffMaxWaitMillis * 1000;
}


uint8_t getBoolAsNum(bool b) {
    if (b)
        return (uint8_t) atoi("1"); // hack because a bool in a memory stored as a uint8_t
    else
        return 0;
}


bool returnSensorDataInJson(uint8_t sensorNumber, char *str, uint32_t strLen, bool pretty = false) {
    bool ret;

    if (sensorNumber < conf.main.sensorsNumber) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        root["sensor"] = sensorNumber;
        root["active"] = getBoolAsNum(conf.sensors[sensorNumber]->active);
        root["pin"] = conf.sensors[sensorNumber]->pin;
        root["vPin"] = conf.sensors[sensorNumber]->vPin;
        root["microsWhenAlarm"] = conf.sensors[sensorNumber]->microsWhenAlarm;
        root["name"] = conf.sensors[sensorNumber]->name;

        if (pretty)
            root.prettyPrintTo(str, strLen);
        else
            root.printTo(str, strLen);

        ret = true;
    } else
        ret = false;

    return ret;
}


void processSensorsStatus() {
    for (i = 0; i < conf.main.sensorsNumber; i++) {
        if (conf.sensors[i]->pin == conf.pinner.ignoredPin)
            sensors.sensor[i].status = OFF;
        else if ((sensors.sensor[i].offMicros <= conf.main.sensorOffNotConnectedMicros) ||
                 (sensors.sensor[i].offMicros >= sensorOffMaxWaitMicros))
            sensors.sensor[i].status = GAP;
        else if (sensors.sensor[i].offMicros <= conf.sensors[i]->microsWhenAlarm)
            sensors.sensor[i].status = LEAK;
        else
            sensors.sensor[i].status = GOOD;
    }
}


bool returnSensorStatusInJson(uint8_t sensorNumber, char *str, uint32_t strLen, bool pretty = false) {
    bool ret;

    if (sensorNumber < conf.main.sensorsNumber) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        root["n"] = sensorNumber;
        root["a"] = getBoolAsNum(conf.sensors[sensorNumber]->active);
        root["T"] = conf.sensors[sensorNumber]->microsWhenAlarm;
        root["t"] = sensors.sensor[sensorNumber].offMicros;

        if (sensors.sensor[sensorNumber].status == OFF)
            root["s"] = "OFF";
        else if (sensors.sensor[sensorNumber].status == GAP)
            root["s"] = "GAP";
        else if (sensors.sensor[sensorNumber].status == LEAK)
            root["s"] = "LEAK";
        else if (sensors.sensor[sensorNumber].status == GOOD)
            root["s"] = "GOOD";

        if (pretty)
            root.prettyPrintTo(str, strLen);
        else
            root.printTo(str, strLen);

        ret = true;
    } else
        ret = false;

    return ret;
}


void sendResponse(const char *data, bool isHtml = false, uint16_t responseCode = 200) {
    if (isHtml)
        server.send(responseCode, "text/html; charset=utf-8", data);
    else
        server.send(responseCode, "text/plain; charset=utf-8", data);
}


bool handleAuthorized() {
    bool authorized = true;

#if defined(configHTTPUsername) && defined(configHTTPPassword)
    if (!server.authenticate(configHTTPUsername, configHTTPPassword)) {
        server.requestAuthentication();
        authorized = false;
    }
#endif

    return authorized;
}


void handleURIRoot() {
    if (handleAuthorized())
        sendResponse(indexPage, true);
}


void handleURIMain() {
    if (handleAuthorized()) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        root["name"] = configWiFiManagerAPName;
        root["id"] = ESP.getChipId();

        uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));
        root["uptime"] = strBuf2;

        root["noActionAfterStartWithinSec"] = conf.main.noActionAfterStartWithinSec;

        root["sensorsNumber"] = conf.main.sensorsNumber;
        root["sensorOffNotConnectedMicros"] = conf.main.sensorOffNotConnectedMicros;
        root["sensorOnMillis"] = conf.main.sensorOnMillis;
        root["sensorOffMaxWaitMillis"] = conf.main.sensorOffMaxWaitMillis;

        root["tapOpen"] = conf.main.tapOpen;
        root["tapClose"] = conf.main.tapClose;
        root["tapStop"] = conf.main.tapStop;
        root["tapActionSec"] = conf.main.tapActionSec;

        root["buzzerPin"] = conf.main.buzzerPin;
        root["buzzerHz"] = conf.main.buzzerHz;
        root["buzzerDurationMillis"] = conf.main.buzzerDurationMillis;

        root["blynkAuthToken"] = conf.main.blynkAuthToken;
        root["blynkHost"] = conf.main.blynkHost;

        if (conf.main.blynkPort != 0)
            root["blynkPort"] = conf.main.blynkPort;
        else
            root["blynkPort"] = "";

        root["blynkSSLFingerprint"] = conf.main.blynkSSLFingerprint;

        root["vPinAlarmStatus"] = conf.main.vPinAlarmStatus;
        root["vPinStatus"] = conf.main.vPinStatus;
        root["vPinAlarmForce"] = conf.main.vPinAlarmForce;
        root["vPinAlarmMute"] = conf.main.vPinAlarmMute;
        root["vPinTapOpen"] = conf.main.vPinTapOpen;
        root["vPinTapClose"] = conf.main.vPinTapClose;
        root["vPinTapStop"] = conf.main.vPinTapStop;

        root["blynkFlag"] = uint8_t(blynkFlag);
        root["blynkConnected"] = uint8_t(Blynk.connected());

        root["inAction"] = uint8_t(inActionFlag);
        root["alarm"] = uint8_t(alarmFlag);
        root["alarmForced"] = uint8_t(alarmForcedFlag);
        root["alarmMuted"] = uint8_t(alarmMuteFlag);

        root["WiFiStatus"] = uint8_t(WiFiConnectedFlag);
        root["freeHeap"] = ESP.getFreeHeap();

        if (server.arg("pretty") == "1")
            root.prettyPrintTo(strBuf, sizeof(strBuf));
        else
            root.printTo(strBuf, sizeof(strBuf));

        sendResponse(strBuf);
    }
}


void handleURISensors() {
    if (handleAuthorized()) {
        bool pretty = false;
        uint8_t sensor;
        uint16_t responseCode = 200;

        retUNumFromReq(pretty, server, "pretty");

        if (retUNumFromReq(sensor, server, "sensor")) {
            //Get one sensor data
            if (sensor < conf.main.sensorsNumber) {
                returnSensorDataInJson(sensor, strBuf, sizeof(strBuf), pretty);
            } else {
                strcopy(strBuf, "Sensor not found");
                responseCode = 404;
            }
            //End Get one sensor data
        } else {
            // All sensors data
            strcopy(strBuf, "[\n");

            for (i = 0; i < conf.main.sensorsNumber; i++) {
                returnSensorDataInJson(i, strBuf2, sizeof(strBuf2), pretty);
                strccat(strBuf, strBuf2);

                if (i != conf.main.sensorsNumber - 1)
                    strccat(strBuf, ",\n");
            }

            strccat(strBuf, "\n]\n");
            // End All sensors data
        }

        sendResponse(strBuf, false, responseCode);
    }
}


void handleURISensorsStatus() {
    if (handleAuthorized()) {
        bool pretty = false;
        retUNumFromReq(pretty, server, "pretty");

        strcopy(strBuf, "[\n");

        for (i = 0; i < conf.main.sensorsNumber; i++) {
            returnSensorStatusInJson(i, strBuf2, sizeof(strBuf2), pretty);
            strccat(strBuf, strBuf2);

            if (i != conf.main.sensorsNumber - 1)
                strccat(strBuf, ",\n");
        }

        strccat(strBuf, "\n]\n");

        sendResponse(strBuf, false);
    }
}


void handleURISet() {
    if (handleAuthorized()) {
        bool mainConfigChanged = false, buzzerChanged = false, blynkChanged = false, sensorConfigChanged = false;
        uint8_t sensorNumber;

        // Main Config
        if (retUNumFromReq(conf.main.noActionAfterStartWithinSec, server, "noActionAfterStartWithinSec"))
            mainConfigChanged = true;

        if (retUNumFromReq(conf.main.sensorsNumber, server, "sensorsNumber"))
            mainConfigChanged = true;

        if (retUNumFromReq(conf.main.sensorOffNotConnectedMicros, server, "sensorOffNotConnectedMicros"))
            mainConfigChanged = true;

        if (retUNumFromReq(conf.main.sensorOnMillis, server, "sensorOnMillis"))
            mainConfigChanged = true;

        if (retUNumFromReq(conf.main.sensorOffMaxWaitMillis, server, "sensorOffMaxWaitMillis"))
            mainConfigChanged = true;

        if (retStrFromReq(conf.main.tapOpen, sizeof(conf.main.tapOpen), server, "tapOpen"))
            mainConfigChanged = true;

        if (retStrFromReq(conf.main.tapClose, sizeof(conf.main.tapClose), server, "tapClose"))
            mainConfigChanged = true;

        if (retStrFromReq(conf.main.tapStop, sizeof(conf.main.tapStop), server, "tapStop"))
            mainConfigChanged = true;

        if (retUNumFromReq(conf.main.tapActionSec, server, "tapActionSec"))
            mainConfigChanged = true;

        // Buzzer
        if (retUNumFromReq(conf.main.buzzerPin, server, "buzzerPin")) {
            conf.pinner.fixPin(conf.main.buzzerPin);
            mainConfigChanged = true;
            buzzerChanged = true;
        }

        if (retStrFromReq(conf.main.buzzerHz, sizeof(conf.main.buzzerHz), server,
                          "buzzerHz")) {
            mainConfigChanged = true;
            buzzerChanged = true;
        }

        if (retStrFromReq(conf.main.buzzerDurationMillis, sizeof(conf.main.buzzerDurationMillis), server,
                          "buzzerDurationMillis")) {
            mainConfigChanged = true;
            buzzerChanged = true;
        }
        // End Buzzer

        // Blynk
        if (retStrFromReq(conf.main.blynkAuthToken, sizeof(conf.main.blynkAuthToken), server,
                          "blynkAuthToken")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retStrFromReq(conf.main.blynkHost, sizeof(conf.main.blynkHost), server,
                          "blynkHost")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.blynkPort, server, "blynkPort")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retStrFromReq(conf.main.blynkSSLFingerprint, sizeof(conf.main.blynkSSLFingerprint), server,
                          "blynkSSLFingerprint")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.vPinAlarmStatus, server, "vPinAlarmStatus")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.vPinStatus, server, "vPinStatus")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.vPinAlarmForce, server, "vPinAlarmForce")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.vPinAlarmMute, server, "vPinAlarmMute")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.vPinTapOpen, server, "vPinTapOpen")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.vPinTapClose, server, "vPinTapClose")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }

        if (retUNumFromReq(conf.main.vPinTapStop, server, "vPinTapStop")) {
            mainConfigChanged = true;
            blynkChanged = true;
        }
        // End Blynk
        // End Main Config

        if (mainConfigChanged) {
            conf.fixMainConfig();
            EEPROMUpdater(conf.main, 0, conf.getEEPROMConfigAddress(0), conf.getEEPROMConfigTotalSize());
            loadConfigs(2, buzzerChanged, blynkChanged);
        }

        // Sensor data
        if (retUNumFromReq(sensorNumber, server, "sensor", true))
            if (sensorNumber < conf.main.sensorsNumber) {

                if (retUNumFromReq(conf.sensors[sensorNumber]->active, server,
                                   "active")) {
                    sensorConfigChanged = true;
                }

                if (retUNumFromReq(conf.sensors[sensorNumber]->pin, server,
                                   "pin"))
                    sensorConfigChanged = true;

                if (retUNumFromReq(conf.sensors[sensorNumber]->vPin, server,
                                   "vPin"))
                    sensorConfigChanged = true;

                if (retUNumFromReq(conf.sensors[sensorNumber]->microsWhenAlarm, server,
                                   "microsWhenAlarm"))
                    sensorConfigChanged = true;

                if (retStrFromReq(conf.sensors[sensorNumber]->name, sizeof(conf.sensors[sensorNumber]->name), server,
                                  "name"))
                    sensorConfigChanged = true;

                if (sensorConfigChanged) {
                    conf.fixSensorConfig(sensorNumber);
                    EEPROMUpdater(*conf.sensors[sensorNumber], 0, conf.getEEPROMConfigAddress(sensorNumber + 1),
                                  conf.getEEPROMConfigTotalSize());
                    Blynk.virtualWrite(conf.sensors[sensorNumber]->vPin, conf.sensors[sensorNumber]->active);
                }
            }
        // End Sensor data

        if (mainConfigChanged || sensorConfigChanged) {
            printConfigs();
            strcopy(strBuf, "OK");
        } else {
            strcopy(strBuf, "Nothing to do");
        }

        sendResponse(strBuf);
    }
}


void handleURIAlarm() {
    if (handleAuthorized()) {
        bool applied = false;
        uint8_t par;

        if (retUNumFromReq(par, server, "mute", true))
            if ((par == 1) && !alarmMuteFlag) {
                blynkVirtualWriteBool(conf.main.vPinAlarmMute, true, alarmMuteFlag);
                applied = true;
            } else if ((par == 0) && alarmMuteFlag) {
                blynkVirtualWriteBool(conf.main.vPinAlarmMute, false, alarmMuteFlag);
                applied = true;
            }

        if (retUNumFromReq(par, server, "force", true))
            if ((par == 1) && !alarmForcedFlag) {
                blynkVirtualWriteBool(conf.main.vPinAlarmForce, true, alarmForcedFlag);
                applied = true;
            } else if ((par == 0) && alarmForcedFlag) {
                blynkVirtualWriteBool(conf.main.vPinAlarmForce, false, alarmForcedFlag);
                applied = true;
            }

        if (applied)
            strcopy(strBuf, "OK");
        else
            strcopy(strBuf, "Nothing to do");

        sendResponse(strBuf);
    }
}


void handleURITapOpen() {
    if (handleAuthorized()) {
        openTap();
        sendResponse("OK");
    }
}


void handleURITapClose() {
    if (handleAuthorized()) {
        closeTap();
        sendResponse("OK");
    }
}


void handleURITapStop() {
    if (handleAuthorized()) {
        stopTap();
        sendResponse("OK");
    }
}


void handleURIReset() {
    if (handleAuthorized()) {
        sendResponse(server.uri().c_str());
        ESP.reset();
    }
}


void handleURIRestart() {
    if (handleAuthorized()) {
        sendResponse(server.uri().c_str());
        ESP.restart();
    }
}


void handleURITest() {
    sendResponse(server.uri().c_str());
}


void startHTTP() {
    lg("Starting HTTP server... ");

    server.on("/", handleURIRoot);
    server.on("/main/", handleURIMain);
    server.on("/sensors/", handleURISensors);
    server.on("/sensors_status/", handleURISensorsStatus);
    server.on("/set/", handleURISet);
    server.on("/alarm/", handleURIAlarm);
    server.on("/tap/open", handleURITapOpen);
    server.on("/tap/close", handleURITapClose);
    server.on("/tap/stop", handleURITapStop);
    server.on("/reset", handleURIReset);
    server.on("/restart", handleURIRestart);
    server.on("/test", handleURITest);

    server.begin();

    log("done");
}


void bgTasks(bool doYield = true, bool doBlynk = true) {
    if (WiFiConnectedFlag) {
#ifdef configOTAisActive
        ArduinoOTA.handle();
#endif
        server.handleClient();

        if (doBlynk && blynkFlag)
            Blynk.run();
    }

    beepFlag = !alarmMuteFlag && (alarmFlag || alarmForcedFlag);

    if ((beepFlag || buzzer.buzzerFlag) &&
        (conf.main.buzzerPin != conf.pinner.ignoredPin) &&
        Elapser::isElapsedTimeFromStart(buzzer.lastBuzzerMillis,
                                        buzzer.buzzerDurationMillis[buzzer.curNote],
                                        milliSec))
        buzzer.beep(beepFlag);

    if (tapInActionFlag && Elapser::isElapsedTimeFromStart(lastTapActionSec, conf.main.tapActionSec, sec))
        stopTap();

    if (doYield)
        yield();
}


void setup() {
    Serial.begin(configSerialSpeed);

    while (!Serial)
        yield();

    log("Start");

    WiFi.onEvent(WiFiEvent);

#ifdef configWiFiManagerIsActive
    log("Connect or setup WiFi");

    if (WiFi.status() != WL_CONNECTED) {
        WiFiManager wifiManager;

        // Reset settings, only for testing! This will reset your current WiFi settings
        //wifiManager.resetSettings();

        wifiManager.setTimeout(configWiFiManagerConfigTimeoutSeconds);
        wifiManager.setAPCallback(APConfigCallback);

        snprintf(strBuf, sizeof(strBuf), "%s-%lu", configWiFiManagerAPName, micros());
        snprintf(strBuf2, sizeof(strBuf2), "%08i", ESP.getChipId());

        if (!wifiManager.autoConnect(strBuf, strBuf2))
            log("Timeout");
    }
#endif

#ifdef configOTAisActive
    ArduinoOTA.begin();
    ArduinoOTA.setHostname(configWiFiManagerAPName);
#ifdef configOTAPassword
    ArduinoOTA.setPassword(configOTAPassword);
#endif
#endif

    startHTTP();
    loadConfigs();
    printConfigs();
}


void loop() {
    // Turn On sensor(s)
    for (i = 0; i < conf.main.sensorsNumber; i++) {
        if (conf.sensors[i]->pin != conf.pinner.ignoredPin) {
            pinMode(conf.sensors[i]->pin, OUTPUT);
            digitalWrite(conf.sensors[i]->pin, HIGH);
        }
    }
    // End Turn On

    lastSensorsOnMicros = micros();

    // Do things while sensors On
    if (processedSensorsNumber > 0) {
        processSensorsStatus();

        // Make sensor(s) status string
        strcopy(sensorsStatus, "Status: ");

        if (alarmFlag)
            strccat(sensorsStatus, "ALERT");
        else
            strccat(sensorsStatus, "GOOD");

        if (!inActionFlag) {
            strccat(sensorsStatus, " (No action)");

            if (Elapser::isElapsedTimeFromStart(0, conf.main.noActionAfterStartWithinSec, sec))
                inActionFlag = true;
        }

        strccat(sensorsStatus, ". Uptime: ");
        j = strlen(sensorsStatus);
        uptimer.returnUptimeStr(sensorsStatus + j, sizeof(sensorsStatus) - j);
        strccat(sensorsStatus, "\n");

        for (i = 0; i < conf.main.sensorsNumber; i++) {
            j = strlen(sensorsStatus);
            returnSensorStatusInJson(i, sensorsStatus + j, sizeof(sensorsStatus) - j, false);
            strccat(sensorsStatus, "\n");
        }
        // End Make sensor(s) status string

        log(sensorsStatus);
        blynkWriteTerminal(sensorsStatus);

        // Do action(s)
        processedSensorsNumber = 0;

        for (i = 0; i < conf.main.sensorsNumber; i++) {
            if ((conf.sensors[i]->active) && (sensors.sensor[i].status != GOOD)) {
                if (inActionFlag && !alarmFlag) {
                    alarmFlag = true;

                    snprintf(strBuf, sizeof(strBuf), "ALERT! Sensor %u: %s", i, conf.sensors[i]->name);
                    log(strBuf);
                    blynkWriteTerminalAndNotify(strBuf);

                    if (blynkFlag)
                        blynkWriteAlarmStatus();

                    closeTap();
                }

                break;
            }

            processedSensorsNumber++;
        }

        if (alarmFlag && (processedSensorsNumber == conf.main.sensorsNumber)) {
            alarmFlag = false;

            if (blynkFlag)
                blynkWriteAlarmStatus();
        }
        // End Do action(s)
    }

    if (Elapser::isElapsedTimeFromStart(lastCronTaskSec, cronTasksEverySec, sec)) {
        uptimer.update();
        lastCronTaskSec = Elapser::getLastTime();
    }
    // End Do things while sensors On

    while (!Elapser::isElapsedTimeFromStart(lastSensorsOnMicros, sensorOnMicros, microSec))
        bgTasks();

    // Turn Off sensor(s)
    for (i = 0; i < conf.main.sensorsNumber; i++)
        if (conf.sensors[i]->pin != conf.pinner.ignoredPin)
            pinMode(conf.sensors[i]->pin, INPUT);

    lastSensorsOffMicros = micros();
    // End Turn Off

    // Calculate sensors Off time
    for (i = 0; i < conf.main.sensorsNumber; i++)
        sensors.sensor[i].processed = false;

    processedSensorsNumber = 0;

    // Ignored sensors are "as" processed
    for (i = 0; i < conf.main.sensorsNumber; i++)
        if (conf.sensors[i]->pin == conf.pinner.ignoredPin) {
            sensors.sensor[i].offMicros = 0;
            sensors.sensor[i].processed = true;
            processedSensorsNumber++;
        }
    // End Ignored sensors are "as" processed

    while (true) {
        for (i = 0; i < conf.main.sensorsNumber; i++)
            if (!sensors.sensor[i].processed &&
                ((digitalRead(conf.sensors[i]->pin) == LOW) ||
                 Elapser::isElapsedTimeFromStart(lastSensorsOffMicros, sensorOffMaxWaitMicros,
                                                 microSec))) {
                sensors.sensor[i].offMicros = Elapser::getElapsedTime(lastSensorsOffMicros);
                sensors.sensor[i].processed = true;
                processedSensorsNumber++;
            }

        if (processedSensorsNumber == conf.main.sensorsNumber)
            break;
    }
    // End Calculate sensors off time

    bgTasks(false);
}
