const char indexPage[] PROGMEM = R"END(<!doctype html>

<html lang="en">
<head>
    <meta charset="utf-8">
    <title>Water Leak Guard</title>

    <style type="text/css">
        table {
            border-collapse: collapse;
            border: #e2e2e2;
        }

        #loading {
            width: 100%;
            height: 100%;
            top: 0px;
            left: 0px;
            position: fixed;
            opacity: 0.6;
            background-color: #fff;
            z-index: 99;
            text-align: center;
        }

        #loading-content {
            position: absolute;
            top: 50%;
            left: 50%;
            text-align: center;
            z-index: 100;
        }

        .hide {
            display: none;
        }

        .red {
            background: red;
            color: white;
        }

        .brown {
            background: brown;
            color: white;
        }

        .green {
            background: green;
            color: white;
        }

        .blue {
            background: blue;
            color: white;
        }

        .yellow {
            background: yellow;
            color: black;
        }

        .palegreen {
            background: palegreen;
            color: black;
        }
    </style>
</head>

<body>
<script>
    var prefix = window.location.search.substring(1);
    var sensorsStatusAutoUpdateActiveFlag = false, autoupdater;
    var httpRequest, httpURL, requestIsActive = false;

    function getFormDataAsKeyValue(f) {
        var kv = [];

        for (var i = 0; i < f.elements.length; i++)
            if (f.elements[i].name.length >= 1)
                kv.push(encodeURIComponent(f.elements[i].name) + "=" + encodeURIComponent(f.elements[i].value));

        return kv.join("&");
    }


    function alertErrorRequest(request_status) {
        alert('There was a problem with the request:\n' + httpURL + '\nRequest status: ' + request_status);
    }


    function onRequest() {
        document.getElementById("loading").className = "";
        requestIsActive = true;
    }


    function offRequest() {
        document.getElementById("loading").className = "hide";
        requestIsActive = false;
    }


    function makeRequest(url, handler, arg) {
        if (requestIsActive === false) {
            httpRequest = new XMLHttpRequest();

            if (!httpRequest) {
                alert('Error. Cannot create an XMLHTTP instance');
                return false;
            }

            httpRequest.onreadystatechange = function () {
                handler(arg);
            };

            onRequest();
            httpURL = url + ((/\?/).test(url) ? "&" : "?") + "nocache=" + (new Date()).getTime();
            httpRequest.open('GET', httpURL);
            httpRequest.send();
        }
    }


    function parseJson(str) {
        return JSON.parse(str);
    }


    // loadOpts: 0 - all
    function loadAllHandler(loadOpts) {
        if (httpRequest.readyState === XMLHttpRequest.DONE) {
            offRequest();

            if (httpRequest.status === 200) {
                var json = parseJson(httpRequest.responseText);

                //Set main data
                for (var el in json) {
                    if (document.getElementById(el)) {
                        if (document.getElementById(el).value)
                            document.getElementById(el).value = json[el];
                        else if ((document.getElementById(el).innerHTML))
                            document.getElementById(el).innerHTML = json[el];
                    }
                    else
                        alert("Element: '" + el + "' does't exist");
                }
                //End Set main data

                if (loadOpts == 0)
                    loadSensors(loadOpts);
            } else {
                alertErrorRequest(httpRequest.status);
            }
        }
    }

    // loadOpts: 0 - all
    function loadSensorsHandler(loadOpts) {
        if (httpRequest.readyState === XMLHttpRequest.DONE) {
            offRequest();

            if (httpRequest.status === 200) {
                var sensorsTable = document.getElementById("sensorsTable");
                var sensorsTableRows = sensorsTable.rows.length;

                for (var i = 1; i < sensorsTableRows; i++)
                    sensorsTable.deleteRow(1);

                var json = parseJson(httpRequest.responseText);

                for (var i = 0; i < json.length; i++) {
                    var sensorForm = 'sensor' + i + 'Form';
                    var rowHTML = '<form id="' + sensorForm + '" onsubmit="return false;"/>';

                    var disabled, j = 0;
                    for (var el in json[i]) {
                        if (j === 0)
                            disabled = " disabled";
                        else
                            disabled = "";

                        rowHTML += '<td align="center"><input form="' + sensorForm + '" name="' + el + '" value="' + json[i][el] + '"' + disabled + '></td>';
                        j++;
                    }

                    rowHTML += '<td align="center"><button class="palegreen" onclick="saveSensor(\'' + sensorForm + '\')">lng-saveSensor</button></td>';

                    var row = sensorsTable.insertRow(i + 1);
                    row.innerHTML = rowHTML;

                    if (loadOpts == 0)
                        reloadSensorsStatus();
                }
            } else {
                alertErrorRequest(httpRequest.status);
            }
        }
    }


    function loadSensorsStatusHandler() {
        if (httpRequest.readyState === XMLHttpRequest.DONE) {
            offRequest();
            sensorsStatusAutoUpdateActiveFlag = false;

            if (httpRequest.status === 200) {
                var sensorsStatusTable = document.getElementById("sensorsStatusTable");
                var sensorsStatusTableRows = sensorsStatusTable.rows.length;

                for (var i = 1; i < sensorsStatusTableRows; i++)
                    sensorsStatusTable.deleteRow(1);

                var json = parseJson(httpRequest.responseText);

                for (var i = 0; i < json.length; i++) {
                    var rowHTML = "";

                    var j = 0;
                    var td_add = ' bgcolor="#BBBBBB"';

                    for (var el in json[i]) {
                        if ((j == 1) && (json[i][el] == "1"))
                            td_add = '';

                        rowHTML += '<td' + td_add + '>' + json[i][el] + '</td>';
                        j++;
                    }

                    var row = sensorsStatusTable.insertRow(i + 1);
                    row.innerHTML = rowHTML;
                }
            } else {
                alertErrorRequest(httpRequest.status);
            }
        }
    }


    function emptyHandler() {
        if (httpRequest.readyState === XMLHttpRequest.DONE) {
            offRequest();

            if (httpRequest.status === 200)
                loadAll(0);
            else
                alertErrorRequest(httpRequest.status);
        }
    }


    function loadAll(loadOpts) {
        makeRequest(prefix + "main/", loadAllHandler, loadOpts);
    }


    function loadSensors(loadOpts) {
        makeRequest(prefix + "sensors/", loadSensorsHandler, loadOpts);
    }


    function reloadSensorsStatus() {
        sensorsStatusAutoUpdateActiveFlag = true;
        makeRequest(prefix + "sensors_status/", loadSensorsStatusHandler);
    }

    function saveMain() {
        makeRequest(prefix + "set/?" + getFormDataAsKeyValue(document.getElementById("mainForm")), emptyHandler);
    }


    function saveSensor(sensorForm) {
        makeRequest(prefix + "set/?" + getFormDataAsKeyValue(document.getElementById(sensorForm)), emptyHandler);
    }


    function startSensorsStatusAutoUpdate() {
        stopSensorsStatusAutoUpdate();

        if (sensorsStatusAutoUpdateActiveFlag === false) {
            reloadSensorsStatus();
            autoupdater = setInterval(reloadSensorsStatus, document.getElementById("sensorsStatusUpdateIntervalMillis").value);
        }
    }


    function alarmForce(force) {
        makeRequest(prefix + "alarm/?force=" + force, emptyHandler);
    }


    function alarmMute(mute) {
        makeRequest(prefix + "alarm/?mute=" + mute, emptyHandler);
    }


    function openTap() {
        makeRequest(prefix + "tap/open", emptyHandler);
    }


    function closeTap() {
        makeRequest(prefix + "tap/close", emptyHandler);
    }


    function stopTap() {
        makeRequest(prefix + "tap/stop", emptyHandler);
    }


    function stopSensorsStatusAutoUpdate() {
        clearTimeout(autoupdater);
    }
</script>

<div id="loading" class="hide">
    <div id="loading-content">
        <progress></progress>
    </div>
</div>

<form id="mainForm" onsubmit="return false;">
    <table border="0">
        <tr valign="top">
            <td>
                <table border="1">
                    <tr>
                        <td align="right">lng-WiFiName</td>
                        <td>
                            <div id="name">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-chipId</td>
                        <td>
                            <div id="id">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-uptimeSec</td>
                        <td>
                            <div id="uptime">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-inAction</td>
                        <td>
                            <div id="inAction">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-WiFiStatus</td>
                        <td>
                            <div id="WiFiStatus">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-blynkFlag</td>
                        <td>
                            <div id="blynkFlag">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-blynkConnected</td>
                        <td>
                            <div id="blynkConnected">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-freeHeap</td>
                        <td>
                            <div id="freeHeap">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-alarm</td>
                        <td>
                            <div id="alarm">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-alarmForced</td>
                        <td>
                            <div id="alarmForced">&nbsp;</div>
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-alarmMuted</td>
                        <td>
                            <div id="alarmMuted">&nbsp;</div>
                        </td>
                    </tr>
                </table>
            </td>
            <td>&nbsp;</td>
            <td>
                <table border="1" width="0%">
                    <tr>
                        <td align="right">lng-noActionAfterStartWithinSec</td>
                        <td><input id="noActionAfterStartWithinSec" name="noActionAfterStartWithinSec"
                                   value="&nbsp;">
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-sensorsNumber</td>
                        <td><input id="sensorsNumber" name="sensorsNumber" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-sensorOnMillis</td>
                        <td><input id="sensorOnMillis" name="sensorOnMillis" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-sensorOffNotConnectedMicros</td>
                        <td><input id="sensorOffNotConnectedMicros" name="sensorOffNotConnectedMicros" value="&nbsp;">
                        </td>
                    </tr>
                    <tr>
                        <td align="right">lng-sensorOffMaxWaitMillis</td>
                        <td><input id="sensorOffMaxWaitMillis" name="sensorOffMaxWaitMillis" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-tapOpen</td>
                        <td><input id="tapOpen" name="tapOpen" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-tapClose</td>
                        <td><input id="tapClose" name="tapClose" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-tapStop</td>
                        <td><input id="tapStop" name="tapStop" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-tapActionSec</td>
                        <td><input id="tapActionSec" name="tapActionSec" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-buzzerPin</td>
                        <td><input id="buzzerPin" name="buzzerPin" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-buzzerHz</td>
                        <td><input id="buzzerHz" name="buzzerHz" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-buzzerDurationMillis</td>
                        <td><input id="buzzerDurationMillis" name="buzzerDurationMillis" value="&nbsp;"></td>
                    </tr>
                    <tr align="center">
                        <td colspan="2">
                            <button class="red" onclick="alarmForce(1)">lng-alarmForceOn</button>
                            <button class="green" onclick="alarmForce(0)">lng-alarmForceOff</button>
                        </td>
                    </tr>
                </table>
            </td>
            <td>&nbsp;</td>
            <td>
                <table border="1" width="0%">
                    <tr>
                        <td align="right">lng-blynkAuthToken</td>
                        <td><input id="blynkAuthToken" name="blynkAuthToken" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-blynkHost</td>
                        <td><input id="blynkHost" name="blynkHost" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-blynkPort</td>
                        <td><input id="blynkPort" name="blynkPort" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-blynkSSLFingerprint</td>
                        <td><input id="blynkSSLFingerprint" name="blynkSSLFingerprint" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-vPinAlarmStatus</td>
                        <td><input id="vPinAlarmStatus" name="vPinAlarmStatus" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-vPinStatus</td>
                        <td><input id="vPinStatus" name="vPinStatus" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-vPinAlarmForce</td>
                        <td><input id="vPinAlarmForce" name="vPinAlarmForce" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-vPinAlarmMute</td>
                        <td><input id="vPinAlarmMute" name="vPinAlarmMute" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-vPinTapOpen</td>
                        <td><input id="vPinTapOpen" name="vPinTapOpen" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-vPinTapClose</td>
                        <td><input id="vPinTapClose" name="vPinTapClose" value="&nbsp;"></td>
                    </tr>
                    <tr>
                        <td align="right">lng-vPinTapStop</td>
                        <td><input id="vPinTapStop" name="vPinTapStop" value="&nbsp;"></td>
                    </tr>
                    <tr align="center">
                        <td colspan="2">
                            <button class="brown" onclick="saveMain()">lng-saveMain</button>
                        </td>
                    </tr>
                </table>
            </td>
            <td>&nbsp;</td>
            <td>
                <table border="1" width="0%">
                    <tr align="center">
                        <td>
                            <button class="red" onclick="alarmMute(1)">lng-alarmMuteOn</button>
                        </td>
                        <td>
                            <button class="green" onclick="alarmMute(0)">lng-alarmMuteOff</button>
                        </td>
                    </tr>
                </table>
                <br>
                <table border="1" width="0%">
                    <tr align="center">
                        <td>
                            <button class="blue" onclick="openTap()">lng-tapOpen</button>
                        </td>
                        <td>
                            <button class="blue" onclick="stopTap()">lng-tapStop</button>
                        </td>
                        <td>
                            <button class="green" onclick="closeTap()">lng-tapClose</button>
                        </td>
                    </tr>
                </table>
            </td>
        </tr>
    </table>
</form>

<br>

<table id="sensorsTable" border="1" width="100%">
    <tr>
        <th>lng-sensorNumber</th>
        <th>lng-sensorActive</th>
        <th>lng-sensorPin</th>
        <th>lng-sensorVPin</th>
        <th>lng-sensorMicrosWhenAlarm</th>
        <th>lng-sensorName</th>
        <th>lng-sensorActions</th>
    </tr>
</table>

<br>

<button class="yellow" onclick="reloadSensorsStatus()">lng-reloadSensorsStatus</button>
<button class="blue" onclick="startSensorsStatusAutoUpdate()">lng-startSensorsStatusAutoUpdate</button>
<button class="green" onclick="stopSensorsStatusAutoUpdate()">lng-stopSensorsStatusAutoUpdate</button>
lng-sensorsUpdateIntervalMicros:
<input id="sensorsStatusUpdateIntervalMillis" name="sensorsStatusUpdateIntervalMillis" value="1000">
<br>

<table id="sensorsStatusTable" border="1" width="100%">
    <tr>
        <th>lng-sensorNumber</th>
        <th>lng-sensorActive</th>
        <th>lng-sensorMicrosWhenAlarm</th>
        <th>lng-sensorOffMicros</th>
        <th>lng-sensorStatus</th>
    </tr>
</table>

<script>
    loadAll(0);
</script>

</body>
</html>
)END";
