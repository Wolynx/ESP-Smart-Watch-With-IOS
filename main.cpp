#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ================= BUTTONS ============== */
#define BTN_UP    12
#define BTN_DOWN  14
#define BTN_OK    13

/* ================= WIFI ================= */
const char* ssid = "Volkan Laptop";
const char* password = "31313131";

/* ================= SERVER =============== */
WebServer server(80);

/* ================= DATA ================= */
int hourNow = 0, minuteNow = 0;
unsigned long lastMinuteTick = 0;

String dateStr = "--.--.----";
String locationStr = "---";
int temperature = 0;
int batteryLevel = 0;

/* ================= NOTIFICATION ========= */
bool notifActive = false;
String notifType = "";
String notifFrom = "";
unsigned long notifTime = 0;
const unsigned long NOTIF_DURATION = 8000;

/* ================= UI STATE ============= */
int currentScreen = 0; // 0 main, 1 wifi
unsigned long okPressTime = 0;
bool okHolding = false;

/* ================= WIFI SCAN ============= */
int wifiCount = 0;
int wifiIndex = 0;
String wifiList[10];

/* ================= DRAW MAIN ============= */
void drawMainScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(dateStr);

  display.setCursor(88, 0);
  display.print(batteryLevel);
  display.print("%");

  display.setTextSize(2);
  display.setCursor(0, 16);
  if (hourNow < 10) display.print("0");
  display.print(hourNow);
  display.print(":");
  if (minuteNow < 10) display.print("0");
  display.print(minuteNow);

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(locationStr);

  display.setCursor(0, 54);
  display.print("Hava: ");
  display.print(temperature);
  display.print(" C");

  display.setCursor(80, 54);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "...");

  display.display();
}

/* ================= DRAW WIFI ============= */
void drawWifiScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WiFi Networks");

  for (int i = 0; i < wifiCount && i < 4; i++) {
    int idx = wifiIndex + i;
    if (idx >= wifiCount) break;

    display.setCursor(0, 14 + i * 12);
    if (idx == wifiIndex) display.print("> ");
    else display.print("  ");

    display.print(wifiList[idx]);
  }

  display.display();
}

/* ================= CLOCK ================= */
void updateClock() {
  if (millis() - lastMinuteTick >= 60000) {
    lastMinuteTick = millis();
    minuteNow++;
    if (minuteNow >= 60) {
      minuteNow = 0;
      hourNow = (hourNow + 1) % 24;
    }
    if (!notifActive && currentScreen == 0) drawMainScreen();
  }
}

/* ================= HTTP ================== */
void handleUpdate() {
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "JSON ERROR");
    return;
  }

  if (doc.containsKey("hour"))     hourNow = doc["hour"];
  if (doc.containsKey("minute"))   minuteNow = doc["minute"];
  if (doc.containsKey("date"))     dateStr = doc["date"].as<String>();
  if (doc.containsKey("battery"))  batteryLevel = doc["battery"];
  if (doc.containsKey("weather"))  temperature = doc["weather"];
  if (doc.containsKey("location")) locationStr = doc["location"].as<String>();

  lastMinuteTick = millis();
  if (!notifActive && currentScreen == 0) drawMainScreen();
  server.send(200, "text/plain", "OK");
}

void handleNotify() {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "JSON ERROR");
    return;
  }

  notifType = doc["type"].as<String>();
  notifFrom = doc["from"].as<String>();

  notifActive = true;
  notifTime = millis();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("MSG");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print(notifFrom);
  display.display();

  server.send(200, "text/plain", "OK");
}

/* ================= WIFI SCAN ============= */
void startWifiScan() {
  WiFi.disconnect();
  delay(100);
  wifiCount = WiFi.scanNetworks();
  if (wifiCount > 10) wifiCount = 10;
  for (int i = 0; i < wifiCount; i++) {
    wifiList[i] = WiFi.SSID(i);
  }
  wifiIndex = 0;
  drawWifiScreen();
}

/* ================= SETUP ================= */
void setup() {
  delay(1500);
  Serial.begin(115200);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  MDNS.begin("espwatch");

  server.on("/update", HTTP_POST, handleUpdate);
  server.on("/notify", HTTP_POST, handleNotify);
  server.begin();

  drawMainScreen();
}

/* ================= LOOP ================== */
void loop() {
  server.handleClient();
  updateClock();

  // OK button logic
  if (!digitalRead(BTN_OK)) {
    if (!okHolding) {
      okHolding = true;
      okPressTime = millis();
    }
    if (millis() - okPressTime > 1000) {
      if (currentScreen == 0) {
        currentScreen = 1;
        startWifiScan();
      } else {
        currentScreen = 0;
        drawMainScreen();
      }
      okHolding = false;
      delay(300);
    }
  } else {
    if (okHolding && millis() - okPressTime < 1000) {
      if (currentScreen == 1 && wifiCount > 0) {
        WiFi.begin(wifiList[wifiIndex].c_str(), "31313131");
        while (WiFi.status() != WL_CONNECTED) delay(300);
        currentScreen = 0;
        drawMainScreen();
      }
    }
    okHolding = false;
  }

  if (currentScreen == 1) {
    if (!digitalRead(BTN_UP) && wifiIndex > 0) {
      wifiIndex--;
      drawWifiScreen();
      delay(200);
    }
    if (!digitalRead(BTN_DOWN) && wifiIndex < wifiCount - 1) {
      wifiIndex++;
      drawWifiScreen();
      delay(200);
    }
  }

  if (notifActive && millis() - notifTime > NOTIF_DURATION) {
    notifActive = false;
    if (currentScreen == 0) drawMainScreen();
  }
}
