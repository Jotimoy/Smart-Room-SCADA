#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// Pin definitions
const int fanPin = D5;    // GPIO14
const int lightPin = D6;  // GPIO12
const int lampPin = D7;   // GPIO13
const bool RELAY_ON = LOW;  // Assume active LOW for relay ON (common for opto-coupled relays)

// Constants
const char* ssid = "your-ssid";  // Replace with your WiFi SSID
const char* password = "your-password";  // Replace with your WiFi password
const IPAddress staticIP(192, 168, 0, 140);
const IPAddress gateway(192, 168, 0, 1);
const IPAddress subnet(255, 255, 255, 0);
const int HISTORY_SIZE = 100;
const unsigned long UPDATE_INTERVAL = 1000;  // 1 second
const unsigned long HISTORY_INTERVAL = 60000;  // 1 minute

// Global objects
ESP8266WebServer server(80);
Adafruit_BMP280 bmp;
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variables
bool fanOn = false;
bool lightOn = false;
bool lampOn = false;
bool autoFanEnabled = false;
float tempThreshold = 25.0;
bool scheduleEnabled = false;
int scheduleHour = 0;
int scheduleMinute = 0;
float temp = 0.0;
float press = 0.0;
uint32_t heap = 0;
int rssi = 0;
int cpuFreq = ESP.getCpuFreqMHz();
uint32_t flashSize = ESP.getFlashChipSize();
String logs = "";
int lastMinute = -1;
unsigned long lastUpdate = 0;
unsigned long lastHistory = 0;

// History structure
struct HistoryEntry {
  uint32_t timestamp;
  float temp;
  float press;
  uint32_t heap;
  int rssi;
};
HistoryEntry history[HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;

// Day names
const char* daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Function prototypes
void handleRoot();
void handleData();
void handleHistory();
void handleControl();
void handleSet();
void handleCsv();
void addLog(String msg);
void updateSensorsAndLogic();
void updateLCD(DateTime now);
void updateRelays();

// Setup
void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Initialize pins
  pinMode(fanPin, OUTPUT);
  pinMode(lightPin, OUTPUT);
  pinMode(lampPin, OUTPUT);
  updateRelays();

  // Initialize BMP280
  if (!bmp.begin(0x76)) {
    addLog("BMP280 init failed!");
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  // Initialize RTC
  if (!rtc.begin()) {
    addLog("RTC init failed!");
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    addLog("RTC time set to compile time.");
  }

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // WiFi setup
  WiFi.mode(WIFI_STA);
  WiFi.config(staticIP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  addLog("WiFi connected. IP: " + WiFi.localIP().toString());

  // Server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);
  server.on("/control", handleControl);
  server.on("/set", handleSet);
  server.on("/csv", handleCsv);
  server.begin();
}

// Loop
void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= UPDATE_INTERVAL) {
    updateSensorsAndLogic();
    lastUpdate = currentMillis;
  }
}

// Functions
void updateSensorsAndLogic() {
  // Read sensors
  float newTemp = bmp.readTemperature();
  if (!isnan(newTemp)) {
    temp = newTemp;
  }
  float newPress = bmp.readPressure() / 100.0;
  if (!isnan(newPress)) {
    press = newPress;
  }
  heap = ESP.getFreeHeap();
  rssi = WiFi.RSSI();

  DateTime now = rtc.now();

  // Scheduling
  int currentMinute = now.minute();
  if (scheduleEnabled && now.hour() == scheduleHour && now.minute() == scheduleMinute && lastMinute != currentMinute) {
    fanOn = true;
    addLog("Scheduled Fan ON");
    lastMinute = currentMinute;
  }

  // Automatic fan control
  if (autoFanEnabled) {
    if (temp >= tempThreshold) {
      fanOn = true;
    } else {
      fanOn = false;
    }
  }

  // Update relays
  updateRelays();

  // Update LCD
  updateLCD(now);

  // Add to history
  unsigned long currentMillis = millis();
  if (currentMillis - lastHistory >= HISTORY_INTERVAL) {
    history[historyIndex].timestamp = now.unixtime();
    history[historyIndex].temp = temp;
    history[historyIndex].press = press;
    history[historyIndex].heap = heap;
    history[historyIndex].rssi = rssi;
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    if (historyCount < HISTORY_SIZE) historyCount++;
    lastHistory = currentMillis;
  }
}

void updateLCD(DateTime now) {
  char buf1[17];
  int h = now.hour() % 12;
  if (h == 0) h = 12;
  const char* ampm = (now.hour() < 12) ? "AM" : "PM";
  const char* dayStr = daysOfWeek[now.dayOfTheWeek()];
  sprintf(buf1, "%2d:%02d%s %3s %02d/%02d", h, now.minute(), ampm, dayStr, now.day(), now.month());

  char buf2[17];
  int t = round(temp);
  int p = round(press);
  sprintf(buf2, "T:%3dC P:%4dhPa", t, p);

  lcd.setCursor(0, 0);
  lcd.print(buf1);
  lcd.setCursor(0, 1);
  lcd.print(buf2);
}

void updateRelays() {
  digitalWrite(fanPin, fanOn ? RELAY_ON : !RELAY_ON);
  digitalWrite(lightPin, lightOn ? RELAY_ON : !RELAY_ON);
  digitalWrite(lampPin, lampOn ? RELAY_ON : !RELAY_ON);
}

void addLog(String msg) {
  DateTime now = rtc.now();
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d ", now.hour(), now.minute(), now.second());
  logs = buf + msg + "\n" + logs;
  if (logs.length() > 1000) {
    logs = logs.substring(0, 1000);
  }
}

void handleRoot() {
  const char* html = R"html(
<!DOCTYPE html>
<html>
<head>
<title>Smart Room SCADA Controller</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body {background-color: #333; color: #fff; font-family: Arial;}
.card {background: #444; border: 1px solid #555; padding: 10px; margin: 10px;}
.grid {display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}
button {background: #555; color: #fff; border: 1px solid #666; padding: 5px;}
.red-alert {color: red; font-weight: bold;}
pre {background: #222; padding: 5px; height: 200px; overflow: auto;}
</style>
</head>
<body>
<h1>Smart Room SCADA Controller</h1>
<div class="grid">
<div class="card">
<h2>Device Controls</h2>
<div id="fanStatus">Fan: OFF</div>
<button onclick="control('fan','on')">Fan ON</button>
<button onclick="control('fan','off')">Fan OFF</button>
<br>
<div id="lightStatus">Light: OFF</div>
<button onclick="control('light','on')">Light ON</button>
<button onclick="control('light','off')">Light OFF</button>
<br>
<div id="lampStatus">Lamp: OFF</div>
<button onclick="control('lamp','on')">Lamp ON</button>
<button onclick="control('lamp','off')">Lamp OFF</button>
</div>
<div class="card">
<h2>Settings</h2>
Auto Fan: <input type="checkbox" id="autoEnable"> Threshold: <input type="number" id="threshold" step="0.1"> <button onclick="setAuto()">Set</button>
<br>
Schedule Fan ON: Hour <input type="number" id="schHour" min="0" max="23"> Min <input type="number" id="schMin" min="0" max="59"> <input type="checkbox" id="schEnable"> <button onclick="setSchedule()">Set</button>
</div>
<div class="card">
<h2>Sensor Data</h2>
Temperature: <span id="temp"></span> °C<br>
Pressure: <span id="press"></span> hPa<br>
Time: <span id="time"></span>
</div>
<div class="card">
<h2>System Monitoring</h2>
Heap: <span id="heap"></span> bytes<br>
RSSI: <span id="rssi"></span> dBm<br>
CPU Freq: <span id="cpu"></span> MHz<br>
Flash Size: <span id="flash"></span> bytes<br>
Uptime: <span id="uptime"></span><br>
<div id="alert" class="red-alert"></div>
</div>
<div class="card">
<h2>Command Console</h2>
<pre id="logs"></pre>
</div>
<div class="card">
<h2>Temperature Trend</h2>
<canvas id="tempChart"></canvas>
</div>
<div class="card">
<h2>Pressure Trend</h2>
<canvas id="pressChart"></canvas>
</div>
<div class="card">
<h2>Heap Trend</h2>
<canvas id="heapChart"></canvas>
</div>
<div class="card">
<h2>RSSI Trend</h2>
<canvas id="rssiChart"></canvas>
</div>
</div>
<button onclick="downloadCSV()">Download CSV</button>
<script>
var tempChart = new Chart('tempChart', {type:'line', data:{labels:[], datasets:[{label:'Temperature (°C)', data:[], borderColor:'red'}]}, options:{scales:{y:{beginAtZero:false}}}});
var pressChart = new Chart('pressChart', {type:'line', data:{labels:[], datasets:[{label:'Pressure (hPa)', data:[], borderColor:'blue'}]}, options:{scales:{y:{beginAtZero:false}}}});
var heapChart = new Chart('heapChart', {type:'line', data:{labels:[], datasets:[{label:'Heap (bytes)', data:[], borderColor:'green'}]}, options:{scales:{y:{beginAtZero:false}}}});
var rssiChart = new Chart('rssiChart', {type:'line', data:{labels:[], datasets:[{label:'RSSI (dBm)', data:[], borderColor:'orange'}]}, options:{scales:{y:{beginAtZero:false}}}});
function control(dev, state) {
  fetch(`/control?device=${dev}&state=${state}`).then(() => updateData());
}
function setAuto() {
  let en = document.getElementById('autoEnable').checked ? 'true' : 'false';
  let th = document.getElementById('threshold').value;
  fetch(`/set?type=auto&enable=${en}&threshold=${th}`);
}
function setSchedule() {
  let en = document.getElementById('schEnable').checked ? 'true' : 'false';
  let h = document.getElementById('schHour').value;
  let m = document.getElementById('schMin').value;
  fetch(`/set?type=schedule&enable=${en}&hour=${h}&minute=${m}`);
}
function downloadCSV() {
  window.location = '/csv';
}
function updateData() {
  fetch('/data').then(res => res.json()).then(d => {
    document.getElementById('temp').innerText = d.temp.toFixed(1);
    document.getElementById('press').innerText = d.press.toFixed(1);
    document.getElementById('time').innerText = d.time_str;
    document.getElementById('heap').innerText = d.heap;
    document.getElementById('rssi').innerText = d.rssi;
    document.getElementById('cpu').innerText = d.cpu;
    document.getElementById('flash').innerText = d.flash;
    document.getElementById('uptime').innerText = d.uptime;
    document.getElementById('logs').innerText = d.logs;
    let alertMsg = '';
    if (d.alert.heap) alertMsg += 'Low Heap! ';
    if (d.alert.rssi) alertMsg += 'Low RSSI! ';
    document.getElementById('alert').innerText = alertMsg;
    document.getElementById('fanStatus').innerText = 'Fan: ' + (d.fanOn ? 'ON' : 'OFF');
    document.getElementById('lightStatus').innerText = 'Light: ' + (d.lightOn ? 'ON' : 'OFF');
    document.getElementById('lampStatus').innerText = 'Lamp: ' + (d.lampOn ? 'ON' : 'OFF');
    document.getElementById('autoEnable').checked = d.autoEnabled;
    document.getElementById('threshold').value = d.threshold;
    document.getElementById('schEnable').checked = d.scheduleEnabled;
    document.getElementById('schHour').value = d.scheduleHour;
    document.getElementById('schMin').value = d.scheduleMinute;
  });
}
function updateCharts() {
  fetch('/history').then(res => res.json()).then(data => {
    let labels = [];
    let temps = [];
    let presss = [];
    let heaps = [];
    let rssis = [];
    data.history.forEach(h => {
      labels.push(new Date(h.timestamp * 1000).toLocaleString());
      temps.push(h.temp);
      presss.push(h.press);
      heaps.push(h.heap);
      rssis.push(h.rssi);
    });
    tempChart.data.labels = labels;
    tempChart.data.datasets[0].data = temps;
    tempChart.update();
    pressChart.data.labels = labels;
    pressChart.data.datasets[0].data = presss;
    pressChart.update();
    heapChart.data.labels = labels;
    heapChart.data.datasets[0].data = heaps;
    heapChart.update();
    rssiChart.data.labels = labels;
    rssiChart.data.datasets[0].data = rssis;
    rssiChart.update();
  });
}
setInterval(updateData, 5000);
setInterval(updateCharts, 60000);
window.onload = () => { updateData(); updateCharts(); }
</script>
</body>
</html>
)html";
  server.send(200, "text/html", html);
}

void handleData() {
  DynamicJsonDocument doc(2048);
  DateTime now = rtc.now();
  char timeBuf[10];
  int h = now.hour() % 12;
  if (h == 0) h = 12;
  sprintf(timeBuf, "%d:%02d %s", h, now.minute(), now.hour() < 12 ? "AM" : "PM");
  doc["time_str"] = timeBuf;
  doc["temp"] = temp;
  doc["press"] = press;
  doc["heap"] = heap;
  doc["rssi"] = rssi;
  doc["cpu"] = cpuFreq;
  doc["flash"] = flashSize;
  unsigned long sec = millis() / 1000;
  int days = sec / 86400;
  int hours = (sec % 86400) / 3600;
  int mins = (sec % 3600) / 60;
  int secs = sec % 60;
  char uptimeBuf[20];
  sprintf(uptimeBuf, "%dd %dh %dm %ds", days, hours, mins, secs);
  doc["uptime"] = uptimeBuf;
  doc["logs"] = logs;
  JsonObject alertObj = doc.createNestedObject("alert");
  alertObj["heap"] = (heap < 20000);
  alertObj["rssi"] = (rssi < -80);
  doc["fanOn"] = fanOn;
  doc["lightOn"] = lightOn;
  doc["lampOn"] = lampOn;
  doc["autoEnabled"] = autoFanEnabled;
  doc["threshold"] = tempThreshold;
  doc["scheduleEnabled"] = scheduleEnabled;
  doc["scheduleHour"] = scheduleHour;
  doc["scheduleMinute"] = scheduleMinute;
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleHistory() {
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("history");
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    JsonObject obj = arr.createNestedObject();
    obj["timestamp"] = history[idx].timestamp;
    obj["temp"] = history[idx].temp;
    obj["press"] = history[idx].press;
    obj["heap"] = history[idx].heap;
    obj["rssi"] = history[idx].rssi;
  }
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleControl() {
  String device = server.arg("device");
  String state = server.arg("state");
  bool newState = (state == "on");
  if (device == "fan") {
    fanOn = newState;
  } else if (device == "light") {
    lightOn = newState;
  } else if (device == "lamp") {
    lampOn = newState;
  }
  addLog(device + " turned " + state);
  updateRelays();
  server.send(200, "text/plain", "OK");
}

void handleSet() {
  String type = server.arg("type");
  if (type == "auto") {
    autoFanEnabled = (server.arg("enable") == "true");
    tempThreshold = server.arg("threshold").toFloat();
    addLog("Auto Fan: " + String(autoFanEnabled ? "enabled" : "disabled") + ", threshold: " + String(tempThreshold));
  } else if (type == "schedule") {
    scheduleEnabled = (server.arg("enable") == "true");
    scheduleHour = server.arg("hour").toInt();
    scheduleMinute = server.arg("minute").toInt();
    addLog("Schedule: " + String(scheduleEnabled ? "enabled" : "disabled") + ", time: " + String(scheduleHour) + ":" + String(scheduleMinute));
  }
  server.send(200, "text/plain", "OK");
}

void handleCsv() {
  String csv = "Timestamp,Temperature,Pressure,Heap,RSSI\n";
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    HistoryEntry h = history[idx];
    DateTime dt(h.timestamp);
    char tbuf[20];
    sprintf(tbuf, "%04d-%02d-%02d %02d:%02d:%02d", dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
    csv += tbuf;
    csv += "," + String(h.temp, 1);
    csv += "," + String(h.press, 1);
    csv += "," + String(h.heap);
    csv += "," + String(h.rssi) + "\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=data.csv");
  server.send(200, "text/csv", csv);
}