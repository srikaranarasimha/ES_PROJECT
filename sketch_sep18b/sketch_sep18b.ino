#include <TM1637Display.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <string.h>

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

#define RTC_SDA 23
#define RTC_SCL 4

TM1637Display displayWhite(CLK1, DIO1);
TM1637Display displayBlack(CLK2, DIO2);
RTC_DS3231 rtc;
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
uint32_t lastBlinkMs = 0;
uint32_t lastConfigBlinkMs = 0;
int lastEncoderClockState = HIGH;
bool blinkState = false;
bool configBlinkOn = true;

const uint32_t DEBOUNCE_MS = 35;
const uint32_t CONFIG_BLINK_MS = 400;
const uint32_t LOW_TIME_THRESHOLD_SECONDS = 30;
const char *gameResult = "No result";

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

void sendGameStatus() {
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
  webSocket.broadcastTXT(message);
}

void setup() {
  Serial.begin(115200);

  displayWhite.setBrightness(0x0f);
  displayBlack.setBrightness(0x0f);

  pinMode(BTN_WHITE, INPUT_PULLUP);
  pinMode(BTN_BLACK, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_PAUSE, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  pinMode(LED_WHITE_PIN, OUTPUT);
  pinMode(LED_BLACK_PIN, OUTPUT);
  pinMode(LED_ALERT_PIN, OUTPUT);
  digitalWrite(LED_WHITE_PIN, LOW);
  digitalWrite(LED_BLACK_PIN, LOW);
  digitalWrite(LED_ALERT_PIN, LOW);

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
  WiFi.setAutoReconnect(true);
  if (strlen(WIFI_SSID) > 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to configured Wi-Fi in the background.");
  } else {
    Serial.println("Wi-Fi credentials not configured; clock is running offline.");
  }

  server.on("/", handleRoot);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  lastEncoderClockState = digitalRead(ENC_CLK);
  lastClockUpdateMs = millis();
  showTime(displayWhite, whiteTimeLeftMs);
  showTime(displayBlack, blackTimeLeftMs);
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
  if (gameState == GAME_OVER) {
    if (wasPressed(btnReset)) resetGame();
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

void commitConfiguration() {
  resetGame();
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
  }
  button.lastReading = reading;
  return pressed;
}

int readEncoderDelta() {
  const int clkState = digitalRead(ENC_CLK);
  if (clkState == lastEncoderClockState) return 0;
  lastEncoderClockState = clkState;
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
