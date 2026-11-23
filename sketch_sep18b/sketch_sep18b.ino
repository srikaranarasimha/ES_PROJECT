#include <TM1637Display.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <string.h>
#include <time.h>

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef ADAFRUIT_IO_USERNAME
#define ADAFRUIT_IO_USERNAME ""
#endif

#ifndef ADAFRUIT_IO_KEY
#define ADAFRUIT_IO_KEY ""
#endif

#ifndef ADAFRUIT_IO_FEED_KEY
#define ADAFRUIT_IO_FEED_KEY ""
#endif

// Pin assignments match the existing prototype wiring.
#define CLK1 18
#define DIO1 19
#define CLK2 25
#define DIO2 26

#define BTN_WHITE 32
#define BTN_BLACK 33
#define BTN_START 27
#define BTN_PAUSE 14
#define BTN_RESET 13

#define ENC_CLK 17
#define ENC_DT 16
#define ENC_SW 5

#define LED_WHITE_PIN 22
#define LED_BLACK_PIN 21
#define LED_ALERT_PIN 2
// GPIO 15 is unused here, but is an ESP32 boot-strap pin.
#define BUZZER_PIN 15

#define RTC_SDA 23
#define RTC_SCL 4

TM1637Display displayWhite(CLK1, DIO1);
TM1637Display displayBlack(CLK2, DIO2);
RTC_DS3231 rtc;
Preferences preferences;
WebServer server(80);
WebSocketsServer webSocket(81);

enum GameState {
  SET_WHITE_MIN,
  SET_WHITE_SEC,
  SET_BLACK_MIN,
  SET_BLACK_SEC,
  SET_WHITE_INC,
  SET_BLACK_INC,
  READY,
  RUNNING,
  PAUSED,
  GAME_OVER
};

struct Button {
  uint8_t pin;
  bool lastReading;
  bool stableState;
  uint32_t lastChangeTime;
};

struct PlayerStats {
  uint32_t moves;
  uint64_t totalThinkingMs;
  uint32_t longestMoveSeconds;
  uint32_t shortestMoveSeconds;
};

struct SleepSnapshot {
  uint32_t magic;
  uint8_t version;
  uint8_t state;
  uint8_t whiteActive;
  uint8_t gameStartLogged;
  int64_t whiteTimeLeftMs;
  int64_t blackTimeLeftMs;
  uint64_t pausedTurnMs;
  PlayerStats whiteStats;
  PlayerStats blackStats;
};

struct TelemetrySnapshot {
  char json[768];
};

Button btnWhite = {BTN_WHITE, HIGH, HIGH, 0};
Button btnBlack = {BTN_BLACK, HIGH, HIGH, 0};
Button btnStart = {BTN_START, HIGH, HIGH, 0};
Button btnPause = {BTN_PAUSE, HIGH, HIGH, 0};
Button btnReset = {BTN_RESET, HIGH, HIGH, 0};
Button btnEncoder = {ENC_SW, HIGH, HIGH, 0};

GameState gameState = READY;
PlayerStats whiteStats = {0, 0, 0, 0};
PlayerStats blackStats = {0, 0, 0, 0};

int whiteSetMinutes = 5;
int whiteSetSeconds = 0;
int blackSetMinutes = 5;
int blackSetSeconds = 0;
int whiteIncrement = 0;
int blackIncrement = 0;

int64_t whiteTimeLeftMs = 5LL * 60 * 1000;
int64_t blackTimeLeftMs = 5LL * 60 * 1000;
bool whiteActive = true;
bool rtcAvailable = false;
bool gameStartLogged = false;
bool wifiWasConnected = false;

uint32_t activeTurnStartMs = 0;
uint64_t pausedTurnMs = 0;
uint32_t lastClockUpdateMs = 0;
uint32_t lastUserActivityMs = 0;
uint32_t lastBlinkMs = 0;
uint32_t lastConfigBlinkMs = 0;
uint32_t lastTelemetryQueuedMs = 0;
int currentDisplayBrightness = -1;
int lastEncoderClockState = HIGH;
bool blinkState = false;
bool configBlinkOn = true;
bool restoredAfterDeepSleep = false;
bool timeoutBuzzerActive = false;
bool telemetryNtpStarted = false;
uint32_t timeoutBuzzerPhaseStartMs = 0;

const uint32_t DEBOUNCE_MS = 35;
const uint32_t CONFIG_BLINK_MS = 400;
const uint32_t LOW_TIME_THRESHOLD_SECONDS = 30;
const uint32_t TIMEOUT_BEEP_DURATION_MS = 300;
const uint16_t TIMEOUT_BEEP_FREQUENCY_HZ = 2400;
const uint32_t LIGHT_SLEEP_AFTER_MS = 2UL * 60 * 1000;
const uint32_t DEEP_SLEEP_AFTER_MS = 5UL * 60 * 1000;
const uint8_t DISPLAY_IDLE_BRIGHTNESS = 2;
const uint8_t DISPLAY_RUNNING_BRIGHTNESS = 5;
const uint32_t SLEEP_SNAPSHOT_MAGIC = 0x434C4B31;
const uint8_t SLEEP_SNAPSHOT_VERSION = 1;
const uint32_t TELEMETRY_INTERVAL_MS = 3000;
const size_t TELEMETRY_VALUE_MAX_BYTES = 1024;
const char *ADAFRUIT_IO_ROOT_CA = R"PEM(-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)PEM";
const gpio_num_t WAKE_BUTTON_GPIO = GPIO_NUM_27;
const char *gameResult = "No result";
QueueHandle_t telemetryQueue = nullptr;

const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#10141e">
  <title>Chess Clock</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #10141e;
      --panel: #191f2c;
      --panel-raised: #202838;
      --text: #f3f5fa;
      --muted: #9da8bb;
      --line: #30394b;
      --green: #63e6a5;
      --gold: #f4c66a;
      --red: #ff7185;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: radial-gradient(ellipse at 50% -20%, #26314a 0, var(--bg) 58%);
      color: var(--text);
      font: 16px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
    }
    .wrap { width: min(1060px, 100% - 32px); margin: 0 auto; padding: 28px 0 42px; }
    header { display: flex; justify-content: space-between; align-items: center; gap: 16px; margin-bottom: 24px; }
    .brand { display: flex; align-items: center; gap: 12px; }
    .mark { display: grid; place-items: center; width: 42px; height: 42px; border-radius: 13px; background: #2a354a; font-size: 23px; }
    h1 { font-size: clamp(20px, 4vw, 27px); letter-spacing: -.03em; margin: 0; }
    .subtitle { color: var(--muted); font-size: 13px; margin-top: 2px; }
    .connection { display: inline-flex; align-items: center; gap: 8px; padding: 8px 12px; border: 1px solid var(--line); border-radius: 999px; color: var(--muted); font-size: 13px; white-space: nowrap; }
    .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--gold); box-shadow: 0 0 12px currentColor; }
    .connection.online .dot { background: var(--green); }
    .connection.offline .dot { background: var(--red); }
    .clocks { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; }
    .player { position: relative; overflow: hidden; padding: clamp(20px, 4vw, 32px); background: linear-gradient(145deg, var(--panel-raised), var(--panel)); border: 1px solid var(--line); border-radius: 22px; transition: border-color .2s, transform .2s, box-shadow .2s; }
    .player.active { border-color: var(--green); box-shadow: 0 0 0 1px #63e6a533, 0 14px 42px #0003; transform: translateY(-2px); }
    .player.low .time { color: var(--gold); }
    .player.flagged .time { color: var(--red); }
    .player-head { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
    .player-name { color: var(--muted); font-size: 13px; font-weight: 750; letter-spacing: .14em; }
    .turn { color: var(--green); font-size: 12px; font-weight: 700; opacity: 0; }
    .active .turn { opacity: 1; }
    .time { margin: 16px 0 18px; font: 700 clamp(58px, 12vw, 104px)/1 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; letter-spacing: -.08em; font-variant-numeric: tabular-nums; }
    .player-meta { display: flex; justify-content: space-between; gap: 12px; color: var(--muted); font-size: 13px; }
    .player-meta strong { color: var(--text); font-weight: 650; }
    .statusbar { display: flex; align-items: center; gap: 10px; margin: 17px 0 25px; padding: 13px 16px; background: #171d29; border: 1px solid var(--line); border-radius: 14px; }
    .state { font-size: 13px; font-weight: 800; letter-spacing: .08em; }
    .result { margin-left: auto; color: var(--muted); font-size: 13px; text-align: right; }
    .section-title { margin: 26px 2px 12px; color: var(--muted); font-size: 12px; font-weight: 750; letter-spacing: .13em; text-transform: uppercase; }
    .grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; }
    .stat { min-height: 91px; padding: 16px; background: var(--panel); border: 1px solid var(--line); border-radius: 15px; }
    .stat-label { color: var(--muted); font-size: 12px; }
    .stat-value { margin-top: 7px; font-size: 20px; font-weight: 720; font-variant-numeric: tabular-nums; }
    .footer { margin-top: 24px; color: var(--muted); font-size: 12px; text-align: center; }
    @media (max-width: 650px) {
      .wrap { width: min(100% - 22px, 520px); padding-top: 18px; }
      .clocks { grid-template-columns: 1fr; gap: 11px; }
      .player { padding: 19px 21px; }
      .time { margin: 12px 0; font-size: clamp(62px, 17vw, 86px); }
      .grid { grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 9px; }
      .stat { min-height: 78px; padding: 12px; }
      .stat-value { font-size: 18px; }
    }
    @media (prefers-reduced-motion: reduce) { *, *::before, *::after { transition: none !important; } }
  </style>
</head>
<body>
  <main class="wrap">
    <header>
      <div class="brand">
        <div class="mark" aria-hidden="true">♞</div>
        <div><h1>Chess Clock</h1><div class="subtitle">Live game board</div></div>
      </div>
      <div class="connection" id="connection"><span class="dot"></span><span id="connectionText">Connecting</span></div>
    </header>

    <section class="clocks" aria-label="Player clocks">
      <article class="player" id="whiteBox">
        <div class="player-head"><span class="player-name">WHITE</span><span class="turn">YOUR TURN</span></div>
        <div class="time" id="whiteTime">05:00</div>
        <div class="player-meta"><span>Moves <strong id="whiteMoves">0</strong></span><span>Increment <strong id="whiteInc">+0s</strong></span></div>
      </article>
      <article class="player" id="blackBox">
        <div class="player-head"><span class="player-name">BLACK</span><span class="turn">YOUR TURN</span></div>
        <div class="time" id="blackTime">05:00</div>
        <div class="player-meta"><span>Moves <strong id="blackMoves">0</strong></span><span>Increment <strong id="blackInc">+0s</strong></span></div>
      </article>
    </section>

    <div class="statusbar">
      <span class="dot" id="stateDot"></span><span class="state" id="status">READY</span>
      <span class="result" id="result">No result</span>
    </div>

    <h2 class="section-title">Game setup</h2>
    <section class="grid" aria-label="Game setup">
      <div class="stat"><div class="stat-label">White starting time</div><div class="stat-value" id="whiteSet">05:00</div></div>
      <div class="stat"><div class="stat-label">Black starting time</div><div class="stat-value" id="blackSet">05:00</div></div>
      <div class="stat"><div class="stat-label">Time control</div><div class="stat-value" id="mode">Sudden death</div></div>
      <div class="stat"><div class="stat-label">RTC</div><div class="stat-value" id="rtc">Unavailable</div></div>
    </section>

    <h2 class="section-title">Thinking-time analytics</h2>
    <section class="grid" aria-label="Thinking-time analytics">
      <div class="stat"><div class="stat-label">White total</div><div class="stat-value" id="whiteTotal">0s</div></div>
      <div class="stat"><div class="stat-label">Black total</div><div class="stat-value" id="blackTotal">0s</div></div>
      <div class="stat"><div class="stat-label">White average / move</div><div class="stat-value" id="whiteAverage">0s</div></div>
      <div class="stat"><div class="stat-label">Black average / move</div><div class="stat-value" id="blackAverage">0s</div></div>
      <div class="stat"><div class="stat-label">White longest move</div><div class="stat-value" id="whiteLongest">0s</div></div>
      <div class="stat"><div class="stat-label">Black longest move</div><div class="stat-value" id="blackLongest">0s</div></div>
      <div class="stat"><div class="stat-label">White shortest move</div><div class="stat-value" id="whiteShortest">0s</div></div>
      <div class="stat"><div class="stat-label">Black shortest move</div><div class="stat-value" id="blackShortest">0s</div></div>
    </section>
    <p class="footer">This page mirrors the physical clock. Use the buttons and encoder on the device to control a game.</p>
  </main>
  <script>
    const connection = document.getElementById('connection');
    let retryDelay = 1000;
    function formatTime(seconds) {
      seconds = Math.max(0, Number(seconds) || 0);
      return String(Math.floor(seconds / 60)).padStart(2, '0') + ':' +
        String(seconds % 60).padStart(2, '0');
    }
    function setText(id, value) { document.getElementById(id).textContent = value; }
    function connect() {
      const ws = new WebSocket('ws://' + location.hostname + ':81/');
      ws.onopen = () => {
        connection.className = 'connection online';
        setText('connectionText', 'Live');
        retryDelay = 1000;
      };
      ws.onclose = () => {
        connection.className = 'connection offline';
        setText('connectionText', 'Reconnecting');
        setTimeout(connect, retryDelay);
        retryDelay = Math.min(retryDelay * 2, 15000);
      };
      ws.onerror = () => ws.close();
      ws.onmessage = event => {
        let data;
        try { data = JSON.parse(event.data); } catch (_) { return; }
        setText('whiteTime', formatTime(data.white));
        setText('blackTime', formatTime(data.black));
        setText('whiteSet', formatTime(data.whiteSetMinutes * 60 + data.whiteSetSeconds));
        setText('blackSet', formatTime(data.blackSetMinutes * 60 + data.blackSetSeconds));
        setText('whiteMoves', data.whiteMoves);
        setText('blackMoves', data.blackMoves);
        setText('whiteInc', '+' + data.whiteIncrement + 's');
        setText('blackInc', '+' + data.blackIncrement + 's');
        setText('whiteTotal', data.whiteTotalThinking + 's');
        setText('blackTotal', data.blackTotalThinking + 's');
        setText('whiteAverage', data.whiteAverageMove + 's');
        setText('blackAverage', data.blackAverageMove + 's');
        setText('whiteLongest', data.whiteLongestMove + 's');
        setText('blackLongest', data.blackLongestMove + 's');
        setText('whiteShortest', data.whiteShortestMove + 's');
        setText('blackShortest', data.blackShortestMove + 's');
        setText('rtc', data.rtcAvailable ? 'Connected' : 'Unavailable');

        const increments = [data.whiteIncrement, data.blackIncrement];
        const mode = increments.every(value => value === 0) ? 'Sudden death' :
          (increments[0] === increments[1] ? 'Fischer +' + increments[0] + 's' : 'Custom increment');
        setText('mode', mode);
        setText('status', data.state);
        setText('result', data.result);

        const white = document.getElementById('whiteBox');
        const black = document.getElementById('blackBox');
        white.classList.toggle('active', data.state === 'RUNNING' && data.whiteActive);
        black.classList.toggle('active', data.state === 'RUNNING' && !data.whiteActive);
        white.classList.toggle('low', data.white <= 30 && data.state === 'RUNNING' && data.whiteActive);
        black.classList.toggle('low', data.black <= 30 && data.state === 'RUNNING' && !data.whiteActive);
        white.classList.toggle('flagged', data.state === 'GAME OVER' && data.white === 0);
        black.classList.toggle('flagged', data.state === 'GAME OVER' && data.black === 0);
        document.getElementById('stateDot').style.background =
          data.state === 'RUNNING' ? 'var(--green)' :
          data.state === 'GAME OVER' ? 'var(--red)' : 'var(--gold)';
      };
    }
    connect();
  </script>
</body>
</html>
)HTML";

bool wasPressed(Button &button);
void synchronizeButtonState(Button &button);
int readEncoderDelta();
void updateSetupDisplay();
void showTime(TM1637Display &display, int64_t millisecondsLeft);
void sendGameStatus();
void resetGame();
void commitConfiguration();
void updateClock(uint32_t now);
void recordMove(uint32_t now);
void updateIndicators(uint32_t now);
void logTimestamp(const char *event);
const char *stateName(GameState state);
uint32_t clockSecondsLeft(int64_t millisecondsLeft);
void startOrResume(uint32_t now);
void loadPersistentState();
bool savePersistentSettings();
bool saveSleepSnapshot();
void updateDisplayBrightness();
void enterIdleSleep();
bool enterDeepSleep();
void restoreAfterLightSleep();
void startTimeoutBuzzer(uint32_t now);
void updateTimeoutBuzzer(uint32_t now);
void stopTimeoutBuzzer();
String buildGameStatusJson();
void queueTelemetryIfDue(uint32_t now);
void telemetryPublisherTask(void *parameter);

void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void webSocketEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length) {
  (void)client;
  (void)payload;
  (void)length;
  if (type == WStype_CONNECTED) {
    sendGameStatus();
  }
}

const char *stateName(GameState state) {
  switch (state) {
    case RUNNING: return "RUNNING";
    case PAUSED: return "PAUSED";
    case GAME_OVER: return "GAME OVER";
    default: return "READY";
  }
}

uint32_t clockSecondsLeft(int64_t millisecondsLeft) {
  if (millisecondsLeft <= 0) return 0;
  return static_cast<uint32_t>((millisecondsLeft + 999) / 1000);
}

String buildGameStatusJson() {
  String message;
  message.reserve(600);
  message = "{";
  message += "\"white\":" + String(clockSecondsLeft(whiteTimeLeftMs)) + ",";
  message += "\"black\":" + String(clockSecondsLeft(blackTimeLeftMs)) + ",";
  message += "\"whiteActive\":" + String(whiteActive ? "true" : "false") + ",";
  message += "\"state\":\"" + String(stateName(gameState)) + "\",";
  message += "\"result\":\"" + String(gameResult) + "\",";
  message += "\"rtcAvailable\":" + String(rtcAvailable ? "true" : "false") + ",";
  message += "\"whiteSetMinutes\":" + String(whiteSetMinutes) + ",";
  message += "\"whiteSetSeconds\":" + String(whiteSetSeconds) + ",";
  message += "\"blackSetMinutes\":" + String(blackSetMinutes) + ",";
  message += "\"blackSetSeconds\":" + String(blackSetSeconds) + ",";
  message += "\"whiteIncrement\":" + String(whiteIncrement) + ",";
  message += "\"blackIncrement\":" + String(blackIncrement) + ",";
  message += "\"whiteMoves\":" + String(whiteStats.moves) + ",";
  message += "\"blackMoves\":" + String(blackStats.moves) + ",";
  message += "\"whiteTotalThinking\":" + String(whiteStats.totalThinkingMs / 1000) + ",";
  message += "\"blackTotalThinking\":" + String(blackStats.totalThinkingMs / 1000) + ",";
  message += "\"whiteAverageMove\":" +
    String(whiteStats.moves ? (whiteStats.totalThinkingMs / whiteStats.moves) / 1000 : 0) + ",";
  message += "\"blackAverageMove\":" +
    String(blackStats.moves ? (blackStats.totalThinkingMs / blackStats.moves) / 1000 : 0) + ",";
  message += "\"whiteLongestMove\":" + String(whiteStats.longestMoveSeconds) + ",";
  message += "\"blackLongestMove\":" + String(blackStats.longestMoveSeconds) + ",";
  message += "\"whiteShortestMove\":" + String(whiteStats.shortestMoveSeconds) + ",";
  message += "\"blackShortestMove\":" + String(blackStats.shortestMoveSeconds);
  message += "}";
  return message;
}

void sendGameStatus() {
  String message = buildGameStatusJson();
  webSocket.broadcastTXT(message);
}

void queueTelemetryIfDue(uint32_t now) {
  if (telemetryQueue == nullptr ||
      strlen(ADAFRUIT_IO_USERNAME) == 0 ||
      strlen(ADAFRUIT_IO_KEY) == 0 ||
      strlen(ADAFRUIT_IO_FEED_KEY) == 0 ||
      WiFi.status() != WL_CONNECTED ||
      static_cast<uint32_t>(now - lastTelemetryQueuedMs) < TELEMETRY_INTERVAL_MS) {
    return;
  }

  const String message = buildGameStatusJson();
  TelemetrySnapshot snapshot = {};
  if (message.length() >= sizeof(snapshot.json)) {
    Serial.println("Adafruit IO snapshot is too large; skipping this update.");
    lastTelemetryQueuedMs = now;
    return;
  }

  message.toCharArray(snapshot.json, sizeof(snapshot.json));
  if (xQueueOverwrite(telemetryQueue, &snapshot) != pdPASS) {
    Serial.println("Could not queue the Adafruit IO snapshot.");
  }
  lastTelemetryQueuedMs = now;
}

void telemetryPublisherTask(void *parameter) {
  (void)parameter;
  TelemetrySnapshot snapshot = {};

  for (;;) {
    if (xQueueReceive(telemetryQueue, &snapshot, portMAX_DELAY) != pdPASS) {
      continue;
    }

    String body;
    body.reserve(strlen(snapshot.json) + 16);
    body = "{\"value\":\"";
    for (const char *cursor = snapshot.json; *cursor != '\0'; ++cursor) {
      if (*cursor == '"' || *cursor == '\\') body += '\\';
      body += *cursor;
    }
    body += "\"}";

    if (body.length() > TELEMETRY_VALUE_MAX_BYTES) {
      Serial.println("Adafruit IO snapshot exceeds the feed's 1 KB value limit.");
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Skipping Adafruit IO update because Wi-Fi is disconnected.");
      continue;
    }
    if (time(nullptr) < 1700000000) {
      if (!telemetryNtpStarted) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        telemetryNtpStarted = true;
      }
      for (uint8_t attempt = 0; attempt < 20 && time(nullptr) < 1700000000; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
      if (time(nullptr) < 1700000000) {
        Serial.println("Skipping Adafruit IO update because network time is not synchronized.");
        continue;
      }
    }

    WiFiClientSecure secureClient;
    secureClient.setCACert(ADAFRUIT_IO_ROOT_CA);

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    const String url = String("https://io.adafruit.com/api/v2/") +
      ADAFRUIT_IO_USERNAME + "/feeds/" + ADAFRUIT_IO_FEED_KEY + "/data";
    if (!http.begin(secureClient, url)) {
      Serial.println("Could not start the Adafruit IO HTTPS request.");
      continue;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-AIO-Key", ADAFRUIT_IO_KEY);
    const int responseCode = http.POST(body);
    if (responseCode < 200 || responseCode >= 300) {
      Serial.printf("Adafruit IO publish failed (HTTP %d).\n", responseCode);
    } else {
      Serial.println("Published clock status to Adafruit IO.");
    }
    http.end();
  }
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);

  loadPersistentState();

  pinMode(BTN_WHITE, INPUT_PULLUP);
  pinMode(BTN_BLACK, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_PAUSE, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  if (restoredAfterDeepSleep) {
    synchronizeButtonState(btnStart);
  }

  pinMode(LED_WHITE_PIN, OUTPUT);
  pinMode(LED_BLACK_PIN, OUTPUT);
  pinMode(LED_ALERT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_WHITE_PIN, LOW);
  digitalWrite(LED_BLACK_PIN, LOW);
  digitalWrite(LED_ALERT_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(RTC_SDA, RTC_SCL);
  Wire.setTimeOut(1000);
  rtcAvailable = rtc.begin();
  if (rtcAvailable && rtc.lostPower()) {
    Serial.println("RTC lost power; setting it to the sketch compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  if (!rtcAvailable) {
    Serial.println("RTC not found; clock will run without timestamps.");
  }

  // Initialize lwIP before binding either server socket.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  if (strlen(WIFI_SSID) > 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to configured Wi-Fi in the background.");
  } else {
    Serial.println("Wi-Fi credentials not configured; clock is running offline.");
  }

  if (strlen(ADAFRUIT_IO_USERNAME) > 0 &&
      strlen(ADAFRUIT_IO_KEY) > 0 &&
      strlen(ADAFRUIT_IO_FEED_KEY) > 0) {
    telemetryQueue = xQueueCreate(1, sizeof(TelemetrySnapshot));
    if (telemetryQueue == nullptr) {
      Serial.println("Could not create the Adafruit IO telemetry queue.");
    } else if (xTaskCreate(telemetryPublisherTask, "aio_publish", 8192, nullptr, 1, nullptr) != pdPASS) {
      Serial.println("Could not start the Adafruit IO publisher task.");
      vQueueDelete(telemetryQueue);
      telemetryQueue = nullptr;
    } else {
      Serial.println("Adafruit IO telemetry publisher is ready.");
    }
  } else {
    Serial.println("Adafruit IO not configured; cloud telemetry is disabled.");
  }

  server.on("/", handleRoot);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  lastEncoderClockState = digitalRead(ENC_CLK);
  lastClockUpdateMs = millis();
  lastUserActivityMs = lastClockUpdateMs;
  updateDisplayBrightness();
  showTime(displayWhite, whiteTimeLeftMs);
  showTime(displayBlack, blackTimeLeftMs);
  if (restoredAfterDeepSleep) {
    Serial.println("Restored the saved idle game state.");
  }
  Serial.printf("CPU frequency: %u MHz\n", getCpuFrequencyMhz());
  enableLoopWDT();
  if (esp_task_wdt_status(nullptr) == ESP_OK) {
    Serial.println("Task watchdog enabled for the main loop (5 second timeout).");
  } else {
    Serial.println("ERROR: task watchdog is not monitoring the main loop.");
  }
  Serial.println("Chess clock ready.");
}

void maintainWifi() {
  if (strlen(WIFI_SSID) == 0) return;
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.print("Dashboard available at http://");
      Serial.println(WiFi.localIP());
    }
    return;
  }
  if (wifiWasConnected) {
    wifiWasConnected = false;
    Serial.println("Wi-Fi disconnected; clock continues offline.");
  }
}

void loop() {
  server.handleClient();
  webSocket.loop();
  const uint32_t now = millis();
  maintainWifi();

  updateClock(now);
  queueTelemetryIfDue(now);
  updateDisplayBrightness();
  if (gameState == GAME_OVER) {
    if (wasPressed(btnReset)) resetGame();
    updateTimeoutBuzzer(millis());
    updateIndicators(now);
    return;
  }

  if (gameState >= SET_WHITE_MIN && gameState <= SET_BLACK_INC) {
    if (now - lastConfigBlinkMs >= CONFIG_BLINK_MS) {
      lastConfigBlinkMs = now;
      configBlinkOn = !configBlinkOn;
      updateSetupDisplay();
    }
    const int delta = readEncoderDelta();
    if (delta != 0) {
      switch (gameState) {
        case SET_WHITE_MIN: whiteSetMinutes = constrain(whiteSetMinutes + delta, 0, 99); break;
        case SET_WHITE_SEC: whiteSetSeconds = constrain(whiteSetSeconds + delta, 0, 59); break;
        case SET_BLACK_MIN: blackSetMinutes = constrain(blackSetMinutes + delta, 0, 99); break;
        case SET_BLACK_SEC: blackSetSeconds = constrain(blackSetSeconds + delta, 0, 59); break;
        case SET_WHITE_INC: whiteIncrement = constrain(whiteIncrement + delta, 0, 60); break;
        case SET_BLACK_INC: blackIncrement = constrain(blackIncrement + delta, 0, 60); break;
        default: break;
      }
      updateSetupDisplay();
      sendGameStatus();
    }
    if (wasPressed(btnEncoder)) {
      switch (gameState) {
        case SET_WHITE_MIN: gameState = SET_WHITE_SEC; break;
        case SET_WHITE_SEC: gameState = SET_BLACK_MIN; break;
        case SET_BLACK_MIN: gameState = SET_BLACK_SEC; break;
        case SET_BLACK_SEC: gameState = SET_WHITE_INC; break;
        case SET_WHITE_INC: gameState = SET_BLACK_INC; break;
        case SET_BLACK_INC:
          commitConfiguration();
          return;
        default: break;
      }
      configBlinkOn = true;
      lastConfigBlinkMs = now;
      updateSetupDisplay();
    }
    return;
  }

  if (wasPressed(btnEncoder)) {
    if (gameState == READY || gameState == PAUSED) {
      gameState = SET_WHITE_MIN;
      configBlinkOn = true;
      lastConfigBlinkMs = now;
      updateSetupDisplay();
      return;
    }
  }

  if (wasPressed(btnStart) && (gameState == READY || gameState == PAUSED)) {
    startOrResume(now);
  }

  if (wasPressed(btnPause) && gameState == RUNNING) {
    pausedTurnMs += static_cast<uint32_t>(now - activeTurnStartMs);
    gameState = PAUSED;
    lastClockUpdateMs = now;
    digitalWrite(LED_WHITE_PIN, LOW);
    digitalWrite(LED_BLACK_PIN, LOW);
    sendGameStatus();
  }

  if (wasPressed(btnReset)) {
    resetGame();
    return;
  }

  if (gameState == RUNNING && wasPressed(btnWhite) && whiteActive) {
    recordMove(now);
  } else if (gameState == RUNNING && wasPressed(btnBlack) && !whiteActive) {
    recordMove(now);
  }

  updateIndicators(now);
  enterIdleSleep();
}

void startOrResume(uint32_t now) {
  if (whiteTimeLeftMs <= 0 || blackTimeLeftMs <= 0) {
    Serial.println("Cannot start: both starting times must be greater than zero.");
    return;
  }
  gameState = RUNNING;
  activeTurnStartMs = now;
  lastClockUpdateMs = now;
  if (!gameStartLogged) {
    logTimestamp("GAME STARTED");
    gameStartLogged = true;
  }
  sendGameStatus();
}

void updateClock(uint32_t now) {
  if (gameState != RUNNING) {
    lastClockUpdateMs = now;
    return;
  }

  const uint32_t elapsedMs = static_cast<uint32_t>(now - lastClockUpdateMs);
  lastClockUpdateMs = now;
  int64_t &activeTime = whiteActive ? whiteTimeLeftMs : blackTimeLeftMs;
  activeTime -= elapsedMs;

  if (activeTime <= 0) {
    activeTime = 0;
    gameState = GAME_OVER;
    gameResult = whiteActive ? "White flagged - Black wins" : "Black flagged - White wins";
    digitalWrite(LED_WHITE_PIN, LOW);
    digitalWrite(LED_BLACK_PIN, LOW);
    digitalWrite(LED_ALERT_PIN, HIGH);
    showTime(displayWhite, whiteTimeLeftMs);
    showTime(displayBlack, blackTimeLeftMs);
    startTimeoutBuzzer(now);
    logTimestamp("GAME OVER");
    Serial.println(gameResult);
    sendGameStatus();
    return;
  }

  static uint32_t lastShownWhite = UINT32_MAX;
  static uint32_t lastShownBlack = UINT32_MAX;
  const uint32_t whiteSeconds = clockSecondsLeft(whiteTimeLeftMs);
  const uint32_t blackSeconds = clockSecondsLeft(blackTimeLeftMs);
  if (whiteSeconds != lastShownWhite || blackSeconds != lastShownBlack) {
    lastShownWhite = whiteSeconds;
    lastShownBlack = blackSeconds;
    showTime(displayWhite, whiteTimeLeftMs);
    showTime(displayBlack, blackTimeLeftMs);
    sendGameStatus();
  }
}

void recordMove(uint32_t now) {
  PlayerStats &stats = whiteActive ? whiteStats : blackStats;
  const uint64_t thinkingMs = pausedTurnMs +
    static_cast<uint32_t>(now - activeTurnStartMs);
  const uint32_t thinkingSeconds = static_cast<uint32_t>(thinkingMs / 1000);

  stats.moves++;
  stats.totalThinkingMs += thinkingMs;
  if (thinkingSeconds > stats.longestMoveSeconds) {
    stats.longestMoveSeconds = thinkingSeconds;
  }
  if (stats.shortestMoveSeconds == 0 || thinkingSeconds < stats.shortestMoveSeconds) {
    stats.shortestMoveSeconds = thinkingSeconds;
  }

  if (whiteActive) {
    whiteTimeLeftMs += static_cast<int64_t>(whiteIncrement) * 1000;
  } else {
    blackTimeLeftMs += static_cast<int64_t>(blackIncrement) * 1000;
  }
  whiteActive = !whiteActive;
  pausedTurnMs = 0;
  activeTurnStartMs = now;
  showTime(displayWhite, whiteTimeLeftMs);
  showTime(displayBlack, blackTimeLeftMs);
  sendGameStatus();
}

void resetGame() {
  stopTimeoutBuzzer();
  whiteTimeLeftMs = (static_cast<int64_t>(whiteSetMinutes) * 60 + whiteSetSeconds) * 1000;
  blackTimeLeftMs = (static_cast<int64_t>(blackSetMinutes) * 60 + blackSetSeconds) * 1000;
  whiteStats = {0, 0, 0, 0};
  blackStats = {0, 0, 0, 0};
  whiteActive = true;
  pausedTurnMs = 0;
  activeTurnStartMs = 0;
  gameStartLogged = false;
  gameResult = "No result";
  gameState = READY;
  lastClockUpdateMs = millis();
  blinkState = false;
  digitalWrite(LED_WHITE_PIN, LOW);
  digitalWrite(LED_BLACK_PIN, LOW);
  digitalWrite(LED_ALERT_PIN, LOW);
  showTime(displayWhite, whiteTimeLeftMs);
  showTime(displayBlack, blackTimeLeftMs);
  sendGameStatus();
}

void startTimeoutBuzzer(uint32_t now) {
  timeoutBuzzerActive = true;
  timeoutBuzzerPhaseStartMs = now;
  tone(BUZZER_PIN, TIMEOUT_BEEP_FREQUENCY_HZ, TIMEOUT_BEEP_DURATION_MS);
}

void updateTimeoutBuzzer(uint32_t now) {
  if (timeoutBuzzerActive &&
      static_cast<uint32_t>(now - timeoutBuzzerPhaseStartMs) >= TIMEOUT_BEEP_DURATION_MS) {
    timeoutBuzzerActive = false;
  }
}

void stopTimeoutBuzzer() {
  if (timeoutBuzzerActive) noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  timeoutBuzzerActive = false;
}

void commitConfiguration() {
  resetGame();
  savePersistentSettings();
}

void loadPersistentState() {
  SleepSnapshot snapshot = {};

  if (!preferences.begin("chessclk", false)) {
    Serial.println("Could not open NVS; using default clock settings.");
    whiteTimeLeftMs = 5LL * 60 * 1000;
    blackTimeLeftMs = 5LL * 60 * 1000;
    return;
  }
  whiteSetMinutes = constrain(preferences.getInt("wmin", whiteSetMinutes), 0, 99);
  whiteSetSeconds = constrain(preferences.getInt("wsec", whiteSetSeconds), 0, 59);
  blackSetMinutes = constrain(preferences.getInt("bmin", blackSetMinutes), 0, 99);
  blackSetSeconds = constrain(preferences.getInt("bsec", blackSetSeconds), 0, 59);
  whiteIncrement = constrain(preferences.getInt("winc", whiteIncrement), 0, 60);
  blackIncrement = constrain(preferences.getInt("binc", blackIncrement), 0, 60);

  const bool hasSnapshot = preferences.getBool("sleep", false);
  const bool snapshotAvailable =
    preferences.getBytesLength("state") == sizeof(snapshot) &&
    preferences.getBytes("state", &snapshot, sizeof(snapshot)) == sizeof(snapshot);

  if (hasSnapshot && snapshotAvailable &&
      snapshot.magic == SLEEP_SNAPSHOT_MAGIC &&
      snapshot.version == SLEEP_SNAPSHOT_VERSION &&
      (snapshot.state == READY || snapshot.state == PAUSED) &&
      snapshot.whiteTimeLeftMs >= 0 && snapshot.blackTimeLeftMs >= 0) {
    gameState = static_cast<GameState>(snapshot.state);
    whiteActive = snapshot.whiteActive != 0;
    gameStartLogged = snapshot.gameStartLogged != 0;
    whiteTimeLeftMs = snapshot.whiteTimeLeftMs;
    blackTimeLeftMs = snapshot.blackTimeLeftMs;
    pausedTurnMs = snapshot.pausedTurnMs;
    whiteStats = snapshot.whiteStats;
    blackStats = snapshot.blackStats;
    restoredAfterDeepSleep = true;
  } else {
    whiteTimeLeftMs =
      (static_cast<int64_t>(whiteSetMinutes) * 60 + whiteSetSeconds) * 1000;
    blackTimeLeftMs =
      (static_cast<int64_t>(blackSetMinutes) * 60 + blackSetSeconds) * 1000;
  }

  if (hasSnapshot && !preferences.putBool("sleep", false)) {
    Serial.println("Could not clear the restored sleep-state marker.");
  }
  preferences.end();
}

bool savePersistentSettings() {
  if (!preferences.begin("chessclk", false)) {
    Serial.println("Could not open NVS to save clock settings.");
    return false;
  }
  bool saved = preferences.putInt("wmin", whiteSetMinutes) == sizeof(int);
  saved = preferences.putInt("wsec", whiteSetSeconds) == sizeof(int) && saved;
  saved = preferences.putInt("bmin", blackSetMinutes) == sizeof(int) && saved;
  saved = preferences.putInt("bsec", blackSetSeconds) == sizeof(int) && saved;
  saved = preferences.putInt("winc", whiteIncrement) == sizeof(int) && saved;
  saved = preferences.putInt("binc", blackIncrement) == sizeof(int) && saved;
  preferences.end();
  if (!saved) Serial.println("Failed to persist one or more clock settings.");
  return saved;
}

bool saveSleepSnapshot() {
  SleepSnapshot snapshot = {};
  snapshot.magic = SLEEP_SNAPSHOT_MAGIC;
  snapshot.version = SLEEP_SNAPSHOT_VERSION;
  snapshot.state = static_cast<uint8_t>(gameState);
  snapshot.whiteActive = whiteActive ? 1 : 0;
  snapshot.gameStartLogged = gameStartLogged ? 1 : 0;
  snapshot.whiteTimeLeftMs = whiteTimeLeftMs;
  snapshot.blackTimeLeftMs = blackTimeLeftMs;
  snapshot.pausedTurnMs = pausedTurnMs;
  snapshot.whiteStats = whiteStats;
  snapshot.blackStats = blackStats;

  if (!preferences.begin("chessclk", false)) {
    Serial.println("Could not open NVS to save paused game before sleep.");
    return false;
  }
  const bool saved =
    preferences.putBytes("state", &snapshot, sizeof(snapshot)) == sizeof(snapshot) &&
    preferences.putBool("sleep", true);
  preferences.end();
  if (!saved) Serial.println("Failed to persist paused game before sleep.");
  return saved;
}

void updateDisplayBrightness() {
  const uint8_t brightness =
    gameState == RUNNING ? DISPLAY_RUNNING_BRIGHTNESS : DISPLAY_IDLE_BRIGHTNESS;
  if (currentDisplayBrightness == brightness) return;
  displayWhite.setBrightness(brightness);
  displayBlack.setBrightness(brightness);
  currentDisplayBrightness = brightness;
}

void enterIdleSleep() {
  if (gameState != READY && gameState != PAUSED) return;

  const uint32_t now = millis();
  const uint32_t idleMs = static_cast<uint32_t>(now - lastUserActivityMs);
  if (idleMs < LIGHT_SLEEP_AFTER_MS) return;
  if (idleMs >= DEEP_SLEEP_AFTER_MS) {
    if (!enterDeepSleep()) restoreAfterLightSleep();
    return;
  }

  const uint32_t remainingMs = DEEP_SLEEP_AFTER_MS - idleMs;

  Serial.println("Idle: entering light sleep; press Start to wake.");
  displayWhite.clear();
  displayBlack.clear();
  displayWhite.setBrightness(7, false);
  displayBlack.setBrightness(7, false);
  currentDisplayBrightness = -1;

  if (strlen(WIFI_SSID) > 0) {
    WiFi.disconnect(false, false);
    wifiWasConnected = false;
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  gpio_wakeup_enable(WAKE_BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  if (remainingMs > 0) {
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(remainingMs) * 1000ULL);
  }

  disableLoopWDT();
  const esp_err_t sleepResult = esp_light_sleep_start();
  enableLoopWDT();
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  gpio_wakeup_disable(WAKE_BUTTON_GPIO);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  if (sleepResult != ESP_OK) {
    Serial.printf("Light sleep failed: %s\n", esp_err_to_name(sleepResult));
    restoreAfterLightSleep();
    return;
  }

  if (wakeCause == ESP_SLEEP_WAKEUP_TIMER &&
      digitalRead(BTN_START) != LOW &&
      (gameState == READY || gameState == PAUSED)) {
    if (!enterDeepSleep()) restoreAfterLightSleep();
    return;
  }

  restoreAfterLightSleep();
  Serial.println("Woke from light sleep.");
}

void restoreAfterLightSleep() {
  synchronizeButtonState(btnStart);
  lastUserActivityMs = millis();
  lastClockUpdateMs = lastUserActivityMs;
  updateDisplayBrightness();
  showTime(displayWhite, whiteTimeLeftMs);
  showTime(displayBlack, blackTimeLeftMs);
  if (strlen(WIFI_SSID) > 0) {
    WiFi.reconnect();
  }
}

bool enterDeepSleep() {
  Serial.println("Idle for 5 minutes: saving game and entering deep sleep.");
  if (!savePersistentSettings() || !saveSleepSnapshot()) {
    Serial.println("Staying awake because deep-sleep state could not be saved.");
    return false;
  }

  digitalWrite(LED_WHITE_PIN, LOW);
  digitalWrite(LED_BLACK_PIN, LOW);
  digitalWrite(LED_ALERT_PIN, LOW);
  displayWhite.clear();
  displayBlack.clear();
  displayWhite.setBrightness(7, false);
  displayBlack.setBrightness(7, false);

  WiFi.mode(WIFI_OFF);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_init(WAKE_BUTTON_GPIO);
  rtc_gpio_set_direction(WAKE_BUTTON_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(WAKE_BUTTON_GPIO);
  rtc_gpio_pulldown_dis(WAKE_BUTTON_GPIO);
  esp_sleep_enable_ext0_wakeup(WAKE_BUTTON_GPIO, 0);
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("Deep sleep did not start.");
  return false;
}

void updateIndicators(uint32_t now) {
  if (gameState != RUNNING) return;

  const uint32_t activeSeconds = clockSecondsLeft(
    whiteActive ? whiteTimeLeftMs : blackTimeLeftMs);
  if (activeSeconds <= LOW_TIME_THRESHOLD_SECONDS) {
    if (now - lastBlinkMs >= 250) {
      lastBlinkMs = now;
      blinkState = !blinkState;
    }
    digitalWrite(LED_WHITE_PIN, whiteActive && blinkState ? HIGH : LOW);
    digitalWrite(LED_BLACK_PIN, !whiteActive && blinkState ? HIGH : LOW);
    digitalWrite(LED_ALERT_PIN, blinkState ? HIGH : LOW);
  } else {
    digitalWrite(LED_WHITE_PIN, whiteActive ? HIGH : LOW);
    digitalWrite(LED_BLACK_PIN, whiteActive ? LOW : HIGH);
    digitalWrite(LED_ALERT_PIN, LOW);
  }
}

bool wasPressed(Button &button) {
  const bool reading = digitalRead(button.pin);
  const uint32_t now = millis();
  if (reading != button.lastReading) {
    button.lastChangeTime = now;
  }

  bool pressed = false;
  if (static_cast<uint32_t>(now - button.lastChangeTime) >= DEBOUNCE_MS &&
      reading != button.stableState) {
    button.stableState = reading;
    pressed = (button.stableState == LOW);
    if (pressed) {
      lastUserActivityMs = now;
    }
  }
  button.lastReading = reading;
  return pressed;
}

void synchronizeButtonState(Button &button) {
  const bool reading = digitalRead(button.pin);
  button.lastReading = reading;
  button.stableState = reading;
  button.lastChangeTime = millis();
}

int readEncoderDelta() {
  const int clkState = digitalRead(ENC_CLK);
  if (clkState == lastEncoderClockState) return 0;
  lastEncoderClockState = clkState;
  lastUserActivityMs = millis();
  return digitalRead(ENC_DT) != clkState ? 1 : -1;
}

void updateSetupDisplay() {
  displayWhite.clear();
  displayBlack.clear();

  TM1637Display *display = &displayWhite;
  int minutes = whiteSetMinutes;
  int seconds = whiteSetSeconds;
  bool settingMinutes = true;
  bool settingTime = true;

  switch (gameState) {
    case SET_WHITE_MIN:
      break;
    case SET_WHITE_SEC:
      settingMinutes = false;
      break;
    case SET_BLACK_MIN:
      display = &displayBlack;
      minutes = blackSetMinutes;
      seconds = blackSetSeconds;
      break;
    case SET_BLACK_SEC:
      display = &displayBlack;
      minutes = blackSetMinutes;
      seconds = blackSetSeconds;
      settingMinutes = false;
      break;
    case SET_WHITE_INC:
      settingTime = false;
      break;
    case SET_BLACK_INC:
      display = &displayBlack;
      settingTime = false;
      break;
    default:
      return;
  }

  if (!settingTime) {
    const int increment = (gameState == SET_WHITE_INC) ? whiteIncrement : blackIncrement;
    if (configBlinkOn) display->showNumberDec(increment, false);
    return;
  }

  uint8_t segments[4];
  const bool showMinutes = !settingMinutes || configBlinkOn;
  const bool showSeconds = settingMinutes || configBlinkOn;
  segments[0] = showMinutes ? display->encodeDigit(minutes / 10) : 0;
  segments[1] = showMinutes ? display->encodeDigit(minutes % 10) : 0;
  segments[2] = showSeconds ? display->encodeDigit(seconds / 10) : 0;
  segments[3] = showSeconds ? display->encodeDigit(seconds % 10) : 0;
  segments[1] |= 0x80;
  display->setSegments(segments, 4, 0);
}

void showTime(TM1637Display &display, int64_t millisecondsLeft) {
  const uint32_t seconds = clockSecondsLeft(millisecondsLeft);
  const uint32_t minutes = seconds / 60;
  const uint32_t remainingSeconds = seconds % 60;
  display.showNumberDecEx(minutes * 100 + remainingSeconds, 0b01000000, true);
}

void logTimestamp(const char *event) {
  Serial.print(event);
  Serial.print(" at: ");
  if (!rtcAvailable) {
    Serial.println("RTC unavailable");
    return;
  }

  const DateTime timestamp = rtc.now();
  char formatted[24];
  snprintf(formatted, sizeof(formatted), "%04u-%02u-%02u %02u:%02u:%02u",
           static_cast<unsigned>(timestamp.year()),
           static_cast<unsigned>(timestamp.month()),
           static_cast<unsigned>(timestamp.day()),
           static_cast<unsigned>(timestamp.hour()),
           static_cast<unsigned>(timestamp.minute()),
           static_cast<unsigned>(timestamp.second()));
  Serial.println(formatted);
}
