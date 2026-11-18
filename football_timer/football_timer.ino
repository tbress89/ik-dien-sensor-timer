/*
  Football Shot Timer — ESP32-C3 SuperMini
  -----------------------------------------
  A player dribbles past the E18-D80NK sensor -> a countdown starts.
  Score before it hits zero.

  On the box:
    - 1-digit 7-segment display shows the seconds / countdown
    - Rotary encoder: turn = set seconds (1-9), press = pause / arm,
      hold 1 s = start the timer by hand (for testing)
  On the phone (optional):
    - Connect to Wi-Fi "FootballTimer", open http://192.168.4.1
    - Live countdown, settings, horn sound

  Board:  "ESP32C3 Dev Module" (esp32 by Espressif), USB CDC On Boot: Enabled
  No extra libraries needed.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "page.h"

// ============================ CONFIG ============================

const char* AP_SSID = "FootballTimer";
const char* AP_PASS = "goal12345";       // at least 8 characters

// ---- pins (ESP32-C3 SuperMini) ----
const int PIN_SENSOR  = 3;               // E18-D80NK black wire (via divider)
const int PIN_BUZZER  = 4;               // buzzer module signal pin
const int PIN_LED     = 8;               // onboard blue LED (active LOW)
const int PIN_ENC_CLK = 10;              // encoder CLK
const int PIN_ENC_DT  = 2;               // encoder DT
const int PIN_ENC_SW  = 9;               // encoder SW (shared with the BOOT button)

// 7-segment display, common anode: segment a, b, c, d, e, f, g
const int PIN_SEG[7] = {0, 1, 5, 6, 7, 20, 21};

// Otronic "Active Buzzer Module" beeps on HIGH  -> true
// TinyTronics "Active Buzzer Module 5V" beeps on LOW -> false
const bool BUZZER_ACTIVE_HIGH = true;

// false: you built the 10k / 20k voltage divider (default, safe)
// true : no divider, sensor has NO built-in pull-up (measured ~0V on black wire)
const bool SENSOR_USE_INTERNAL_PULLUP = false;

// Turning the knob goes the wrong way? Set this to true.
const bool ENCODER_REVERSE = false;

const uint8_t  MAX_SECONDS       = 9;    // one digit -> 1..9 s
const uint32_t TRIGGER_STABLE_MS = 5;    // sensor must see something this long
const uint32_t ARM_CLEAR_MS      = 1000; // sensor must be clear this long before it can trigger
const uint32_t REARM_DELAY_MS    = 2500; // after time up, wait this long, then get ready again
const uint32_t LONG_PRESS_MS     = 1000; // hold the knob this long = manual start
const uint32_t ADJUST_SHOW_MS    = 2000; // after turning the knob: blink, then save

// ================================================================

WebServer server(80);
Preferences prefs;

enum State : uint8_t { ST_IDLE, ST_ARMED, ST_RUNNING, ST_DONE };
struct Beep { uint16_t on, off; };   // declared up here for Arduino's auto-prototypes

const char* stateName(State s) {
  switch (s) {
    case ST_IDLE:    return "idle";
    case ST_ARMED:   return "armed";
    case ST_RUNNING: return "running";
    default:         return "done";
  }
}

// ---- settings (saved in flash) ----
struct Settings {
  uint8_t durationS = 5;
  bool ticks = true;     // beep on the last 3 seconds
  bool buzzer = true;    // buzzer on the device
} cfg;

// ---- state ----
State state = ST_IDLE;
bool ready = false;          // armed AND sensor has been clear long enough
uint32_t runStart = 0;
uint32_t doneAt = 0;
uint32_t lastTickSec = 99;

bool sensorRaw = false;      // true = sensor sees something
uint32_t detectStart = 0;
uint32_t clearStart = 0;

uint32_t eventId = 0;
const char* eventType = "";

// ============================ BUZZER ============================
// Non-blocking beep patterns so the web server and sensor keep running.

Beep pattern[6];
uint8_t patLen = 0, patIdx = 0;
bool patOn = false;
uint32_t patT = 0;

void buzzerWrite(bool on) {
  if (!cfg.buzzer) on = false;
  digitalWrite(PIN_BUZZER, (on == BUZZER_ACTIVE_HIGH) ? HIGH : LOW);
}

void playPattern(const Beep* p, uint8_t n) {
  if (n > 6) n = 6;
  memcpy(pattern, p, n * sizeof(Beep));
  patLen = n;
  patIdx = 0;
  patOn = true;
  patT = millis();
  buzzerWrite(true);
}

void updateBuzzer() {
  if (patIdx >= patLen) return;
  uint32_t now = millis();
  if (patOn) {
    if (now - patT >= pattern[patIdx].on) {
      buzzerWrite(false);
      patOn = false;
      patT = now;
    }
  } else if (now - patT >= pattern[patIdx].off) {
    patIdx++;
    if (patIdx < patLen) {
      buzzerWrite(true);
      patOn = true;
      patT = now;
    }
  }
}

const Beep BEEP_START[]   = {{120, 0}};
const Beep BEEP_TICK[]    = {{60, 0}};
const Beep BEEP_TIMEOUT[] = {{1000, 0}};
const Beep BEEP_TEST[]    = {{80, 60}, {80, 60}, {300, 0}};
const Beep BEEP_ARMED[]   = {{40, 0}};
const Beep BEEP_CLICK[]   = {{8, 0}};

// ============================ DISPLAY ===========================
// Bits: 0=a 1=b 2=c 3=d 4=e 5=f 6=g
//    aaa
//   f   b
//    ggg
//   e   c
//    ddd

const uint8_t DIGITS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
const uint8_t SEG_DASH  = 0x40;   // -
const uint8_t SEG_P     = 0x73;   // P (paused)
const uint8_t SEG_BLANK = 0x00;

uint8_t shownSegs = 0xFF;

void showSegments(uint8_t segs) {
  if (segs == shownSegs) return;
  shownSegs = segs;
  for (int i = 0; i < 7; i++) {
    // common anode: LOW = segment on
    digitalWrite(PIN_SEG[i], (segs >> i) & 1 ? LOW : HIGH);
  }
}

// Lights a, b, c ... g one after another so you can check the wiring.
void displaySelfTest() {
  for (int i = 0; i < 7; i++) {
    showSegments(1 << i);
    delay(150);
  }
  showSegments(0x7F);
  delay(400);
  showSegments(SEG_BLANK);
}

// ============================ ENCODER ===========================

volatile uint8_t encPrev = 3;
volatile int8_t encAcc = 0;
volatile int16_t encSteps = 0;

// Valid quadrature transitions: +1 / -1, invalid (bounce) = 0
const int8_t ENC_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR onEncoder() {
  uint8_t cur = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  if (cur == encPrev) return;
  encAcc += ENC_TABLE[(encPrev << 2) | cur];
  encPrev = cur;
  if (cur == 3) {                 // back in the rest position = one click
    if (encAcc >= 2) encSteps = encSteps + 1;
    else if (encAcc <= -2) encSteps = encSteps - 1;
    encAcc = 0;
  }
}

bool btnDown = false;
bool btnLongFired = false;
uint32_t btnChangeAt = 0;
uint32_t btnDownAt = 0;

uint32_t adjustAt = 0;            // last time the knob was turned
bool adjustPending = false;       // settings changed but not saved yet

// ============================ LOGIC =============================

void saveSettings();

void fireEvent(const char* type) {
  eventId++;
  eventType = type;
  Serial.printf("[event %lu] %s\n", (unsigned long)eventId, type);
}

void setArmed() {
  state = ST_ARMED;
  ready = false;   // becomes true once the sensor is clear for ARM_CLEAR_MS
}

void setIdle() {
  state = ST_IDLE;   // cancels a running attempt without recording it
  ready = false;
}

void startAttempt() {
  state = ST_RUNNING;
  runStart = millis();
  lastTickSec = 99;
  adjustAt = 0;
  fireEvent("start");
  playPattern(BEEP_START, 1);
}

void finishAttempt() {   // time is up
  state = ST_DONE;
  doneAt = millis();
  fireEvent("timeout");
  playPattern(BEEP_TIMEOUT, 1);
}

void readSensor(uint32_t now) {
  bool raw = digitalRead(PIN_SENSOR) == LOW;   // NPN output pulls LOW on detection
  if (raw && !sensorRaw) detectStart = now;
  if (!raw && sensorRaw) clearStart = now;
  sensorRaw = raw;
}

void handleInputs(uint32_t now) {
  // ---- knob rotation: change seconds (not while the clock runs) ----
  noInterrupts();
  int16_t steps = encSteps;
  encSteps = 0;
  interrupts();
  if (ENCODER_REVERSE) steps = -steps;

  if (steps != 0 && state != ST_RUNNING) {
    int v = constrain((int)cfg.durationS + steps, 1, (int)MAX_SECONDS);
    if (v != cfg.durationS) {
      cfg.durationS = v;
      adjustPending = true;
      playPattern(BEEP_CLICK, 1);
    }
    adjustAt = now;
  }
  if (adjustPending && now - adjustAt >= ADJUST_SHOW_MS) {
    saveSettings();             // save once you stop turning (spares the flash)
    adjustPending = false;
  }

  // ---- knob button: short = pause/arm, long = manual start ----
  bool down = digitalRead(PIN_ENC_SW) == LOW;
  if (down != btnDown && now - btnChangeAt > 30) {   // 30 ms debounce
    btnDown = down;
    btnChangeAt = now;
    if (down) {
      btnDownAt = now;
      btnLongFired = false;
    } else if (!btnLongFired) {                       // short press released
      if (state == ST_IDLE) setArmed();
      else setIdle();
    }
  }
  if (btnDown && !btnLongFired && now - btnDownAt >= LONG_PRESS_MS) {
    btnLongFired = true;
    if (state != ST_RUNNING) startAttempt();
  }
}

void updateLogic() {
  uint32_t now = millis();
  readSensor(now);
  handleInputs(now);

  bool detected = sensorRaw && (now - detectStart >= TRIGGER_STABLE_MS);
  uint32_t clearFor = sensorRaw ? 0 : now - clearStart;

  switch (state) {
    case ST_IDLE:
      break;

    case ST_ARMED:
      if (!ready && !sensorRaw && clearFor >= ARM_CLEAR_MS) {
        ready = true;
        playPattern(BEEP_ARMED, 1);
      }
      if (ready && detected) startAttempt();
      break;

    case ST_RUNNING: {
      uint32_t durMs = (uint32_t)cfg.durationS * 1000;
      uint32_t elapsed = now - runStart;
      if (elapsed >= durMs) {
        finishAttempt();
        break;
      }
      uint32_t remaining = durMs - elapsed;
      uint32_t sec = (remaining + 999) / 1000;   // 3, 2, 1
      if (sec < lastTickSec) {
        if (cfg.ticks && sec <= 3 && elapsed > 200) playPattern(BEEP_TICK, 1);
        lastTickSec = sec;
      }
      break;
    }

    case ST_DONE:
      if (now - doneAt >= REARM_DELAY_MS) setArmed();
      break;
  }
}

void updateDisplay() {
  uint32_t now = millis();
  bool blinkFast = (now / 150) % 2 == 0;
  bool blinkSlow = (now / 400) % 2 == 0;
  uint8_t segs;

  if (state != ST_RUNNING && adjustAt && now - adjustAt < ADJUST_SHOW_MS) {
    segs = blinkFast ? DIGITS[cfg.durationS] : SEG_BLANK;      // setting seconds
  } else {
    switch (state) {
      case ST_IDLE:
        segs = SEG_P;
        break;
      case ST_ARMED:
        segs = ready ? DIGITS[cfg.durationS] : SEG_DASH;       // "-" = clear the sensor
        break;
      case ST_RUNNING: {
        uint32_t durMs = (uint32_t)cfg.durationS * 1000;
        uint32_t el = now - runStart;
        uint32_t rem = el >= durMs ? 0 : durMs - el;
        segs = DIGITS[min((uint32_t)9, (rem + 999) / 1000)];
        break;
      }
      default:  // ST_DONE
        segs = blinkSlow ? DIGITS[0] : SEG_BLANK;              // time up
        break;
    }
  }
  showSegments(segs);

  // onboard LED: solid while running, slow blink when ready
  bool led = state == ST_RUNNING || (state == ST_ARMED && ready && blinkSlow);
  digitalWrite(PIN_LED, led ? LOW : HIGH);
}

// ============================ SETTINGS ==========================

void loadSettings() {
  prefs.begin("ftimer", true);
  cfg.durationS = prefs.getUChar("dur", 5);
  cfg.ticks     = prefs.getBool("ticks", true);
  cfg.buzzer    = prefs.getBool("buzz", true);
  prefs.end();
  if (cfg.durationS < 1 || cfg.durationS > MAX_SECONDS) cfg.durationS = 5;
}

void saveSettings() {
  prefs.begin("ftimer", false);
  prefs.putUChar("dur", cfg.durationS);
  prefs.putBool("ticks", cfg.ticks);
  prefs.putBool("buzz", cfg.buzzer);
  prefs.end();
}

// ============================ WEB ===============================

void sendJson(const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}

String stateJson() {
  uint32_t now = millis();
  uint32_t durMs = (uint32_t)cfg.durationS * 1000;
  uint32_t rem = durMs;
  if (state == ST_RUNNING) {
    uint32_t el = now - runStart;
    rem = el >= durMs ? 0 : durMs - el;
  }

  String j;
  j.reserve(300);
  j += "{\"st\":\"";  j += stateName(state);
  j += "\",\"ready\":"; j += ready ? "true" : "false";
  j += ",\"sensor\":";  j += sensorRaw ? "true" : "false";
  j += ",\"rem\":";     j += rem;
  j += ",\"dur\":";     j += durMs;
  j += ",\"max\":";     j += MAX_SECONDS;
  j += ",\"ev\":";      j += eventId;
  j += ",\"evt\":\"";   j += eventType;
  j += "\",\"set\":{\"dur\":"; j += cfg.durationS;
  j += ",\"ticks\":";   j += cfg.ticks ? "true" : "false";
  j += ",\"buzz\":";    j += cfg.buzzer ? "true" : "false";
  j += "}}";
  return j;
}

void handleCmd() {
  String c = server.arg("c");
  if (c == "start") {
    if (state != ST_RUNNING) startAttempt();
  } else if (c == "arm") {
    if (state != ST_RUNNING) setArmed();
  } else if (c == "pause") {
    setIdle();
  } else if (c == "test") {
    playPattern(BEEP_TEST, 3);
  } else {
    server.send(400, "text/plain", "unknown command");
    return;
  }
  sendJson(stateJson());
}

void handleSettings() {
  if (state == ST_RUNNING) {          // don't change the clock mid-attempt
    sendJson(stateJson());
    return;
  }
  if (server.hasArg("dur"))   cfg.durationS = constrain(server.arg("dur").toInt(), 1, (long)MAX_SECONDS);
  if (server.hasArg("ticks")) cfg.ticks     = server.arg("ticks") == "1";
  if (server.hasArg("buzz"))  cfg.buzzer    = server.arg("buzz") == "1";
  saveSettings();
  sendJson(stateJson());
}

// ============================ SETUP / LOOP ======================

void setup() {
  Serial.begin(115200);

  pinMode(PIN_SENSOR, SENSOR_USE_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  for (int i = 0; i < 7; i++) pinMode(PIN_SEG[i], OUTPUT);

  buzzerWrite(false);
  digitalWrite(PIN_LED, HIGH);
  loadSettings();
  displaySelfTest();

  encPrev = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), onEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), onEncoder, CHANGE);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  // Many SuperMini boards have a weak antenna design and drop Wi-Fi at full power.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", PAGE); });
  server.on("/state", HTTP_GET, [] { sendJson(stateJson()); });
  server.on("/cmd", HTTP_POST, handleCmd);
  server.on("/settings", HTTP_POST, handleSettings);
  server.onNotFound([] {   // any other URL -> back to the main page
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
  server.begin();

  clearStart = millis();
  setArmed();
  Serial.printf("Wi-Fi \"%s\" up. Open http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

void loop() {
  updateLogic();
  updateBuzzer();
  updateDisplay();
  server.handleClient();
}
