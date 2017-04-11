#include <Arduino.h>


#define strcopy(a, b) strcpy_safe(a,b,sizeof(a))
#define strccat(a, b) strcat_safe(a,b,sizeof(a))


bool strcpy_safe(char *dst, const char *src, uint32_t dstSize) {
    bool ret;

    if (strlen(src) >= dstSize) {
        strncpy(dst, src, dstSize - 1);
        dst[dstSize - 1] = 0;
        ret = false;
    } else {
        strcpy(dst, src);
        ret = true;
    }

    return ret;
}


bool strcat_safe(char *dst, const char *src, uint32_t dstSize) {
    bool ret;
    uint32_t dstSizeLeft = dstSize - strlen(dst);

    if (strlen(src) >= dstSizeLeft) {
        strncat(dst, src, dstSizeLeft - 1);
        dst[dstSize - 1] = 0;
        ret = false;
    } else {
        strcat(dst, src);
        ret = true;
    }

    return ret;
}


void lg(const char s[]) {
    Serial.print(s);
}


void log(const char s[] = "") {
    Serial.println(s);
}


template<typename T>
bool retUNumFromReq(T &result, ESP8266WebServer &server, const char *webArg, bool ignoreTheSameResult = false) {
    bool res = server.hasArg(webArg);

    if (res) {
        T temp = strtoul(server.arg(webArg).c_str(), NULL, 10);

        if (result != temp)
            result = temp;
        else if (!ignoreTheSameResult)
            res = false;
    }

    return res;
}


bool retStrFromReq(char *str, uint32_t strLen, ESP8266WebServer &server, const char *webArg,
                   bool ignoreTheSameResult = false) {
    bool res = server.hasArg(webArg);

    if (res)
        if (strcmp(str, server.arg(webArg).c_str()) != 0)
            strlcpy(str, server.arg(webArg).c_str(), strLen);
        else if (!ignoreTheSameResult)
            res = false;

    return res;
}


template<typename T>
void returnUNumArrayFromCommaSeparatedStr(T *out, uint32_t outLen, char *str) {
    uint32_t strLen = strlen(str);
    char tmpStr[20];
    uint8_t curTmpStrPos, sizeofTmpStr = sizeof(tmpStr) - 1;
    uint8_t curStrPos = 0, curOutLen = 0;

    while (outLen > curOutLen) {
        curTmpStrPos = 0;

        while ((str[curStrPos] != ',') && (strLen > curStrPos) && (sizeofTmpStr > curTmpStrPos)) {
            tmpStr[curTmpStrPos] = str[curStrPos];
            curStrPos++;
            curTmpStrPos++;
        }

        tmpStr[curTmpStrPos] = 0;
        curStrPos++;

        out[curOutLen] = strtoul(tmpStr, NULL, 10);
        curOutLen++;
    }
}


template<typename T>
void returnCSVStrFromUNumArray(char *str, uint32_t strLen, T *a, uint32_t aLen, bool add = true) {
    if (!add)
        str[0] = 0;

    uint32_t sLen = strlen(str);

    for (uint32_t i = 0; i < aLen; i++) {
        sLen += snprintf(str + sLen, strLen - sLen, "%lu", a[i]);

        if (i != aLen - 1)
            sLen += snprintf(str + sLen, strLen - sLen, ",");
    }
}
