#include <TM1637Display.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
RTC_DS3231 rtc;
bool rtcAvailable = false; // tracks whether RTC calls are safe to make


const char* WIFI_SSID = "IIITS_Student";
const char* WIFI_PASSWORD = "iiit5@2k18";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
// Displays
#define CLK1 18
#define DIO1 19
TM1637Display displayWhite(CLK1, DIO1);

#define CLK2 25
#define DIO2 26
TM1637Display displayBlack(CLK2, DIO2);

// Buttons
#define BTN_WHITE   32
#define BTN_BLACK   33
#define BTN_START   27
#define BTN_PAUSE   14
#define BTN_RESET   13

// Rotary encoder
#define ENC_CLK 17
#define ENC_DT  16
#define ENC_SW  5

// LEDs
#define LED_WHITE_PIN  22
#define LED_BLACK_PIN  21
#define LED_ALERT_PIN  2

// I2C pins for RTC (avoids strapping pins 12/15)
#define RTC_SDA 23
#define RTC_SCL 4

// Game state
enum GameState {
  SET_WHITE_MIN, SET_WHITE_SEC,
  SET_BLACK_MIN, SET_BLACK_SEC,
  SET_WHITE_INC, SET_BLACK_INC,
  READY, RUNNING, PAUSED, GAME_OVER
};
GameState gameState = READY;

// Setup values
int whiteSetMinutes = 5;
int whiteSetSeconds = 0;
int blackSetMinutes = 5;
int blackSetSeconds = 0;
int whiteIncrement = 0;
int blackIncrement = 0;

// Actual running clocks (in seconds)
long whiteTimeLeft = 5L * 60;
long blackTimeLeft = 5L * 60;
bool whiteActive = true;
int whiteMoveCount = 0;
int blackMoveCount = 0;

unsigned long lastTickMillis = 0;
const long LOW_TIME_THRESHOLD = 30; // seconds

bool gameStartLogged = false;

struct Button {
  uint8_t pin;
  bool lastReading;
  bool stableState;
  unsigned long lastChangeTime;
};

Button btnWhite = {BTN_WHITE, HIGH, HIGH, 0};
Button btnBlack = {BTN_BLACK, HIGH, HIGH, 0};
Button btnStart = {BTN_START, HIGH, HIGH, 0};
Button btnPause = {BTN_PAUSE, HIGH, HIGH, 0};
Button btnReset = {BTN_RESET, HIGH, HIGH, 0};
Button btnEncSW = {ENC_SW, HIGH, HIGH, 0};

const unsigned long DEBOUNCE_DELAY = 40;
int lastClkState;

unsigned long lastBlinkMillis = 0;
bool blinkState = false;

// --- Config-screen blink state ---
unsigned long lastConfigBlinkMillis = 0;
bool configBlinkOn = true;
const unsigned long CONFIG_BLINK_INTERVAL = 400; // ms
void handleRoot() {
  server.send(200, "text/html",
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>ESP32 Chess Clock</title>"

    "<style>"
    "body{"
      "font-family:Arial,sans-serif;"
      "text-align:center;"
      "background:#111;"
      "color:white;"
      "margin:0;"
      "padding:30px;"
    "}"

    "h1{font-size:32px;}"

    ".clock{"
      "font-size:70px;"
      "font-weight:bold;"
      "margin:20px;"
    "}"

    ".player{"
      "padding:20px;"
      "margin:15px auto;"
      "max-width:500px;"
      "border:2px solid #555;"
      "border-radius:15px;"
    "}"

    ".active{"
      "border-color:white;"
    "}"

    "#status{"
      "font-size:24px;"
      "margin:25px;"
    "}"

  "#connection{"
  "font-size:16px;"
  "color:#aaa;"
"}"

".info{"
  "max-width:500px;"
  "margin:25px auto;"
  "padding:20px;"
  "border:2px solid #555;"
  "border-radius:15px;"
  "text-align:left;"
"}"

".info h2{"
  "text-align:center;"
  "margin-top:0;"
"}"

".info p{"
  "font-size:18px;"
  "margin:12px 0;"
"}"

".info span{"
  "font-weight:bold;"
  "float:right;"
"}"

"</style>"

    "</head>"

    "<body>"

    "<h1>ESP32 Chess Clock</h1>"

    "<div id='connection'>Connecting...</div>"

    "<div class='player' id='whiteBox'>"
      "<h2>WHITE</h2>"
      "<div class='clock' id='whiteTime'>05:00</div>"
    "</div>"

    "<div class='player' id='blackBox'>"
      "<h2>BLACK</h2>"
      "<div class='clock' id='blackTime'>05:00</div>"
    "</div>"

    "<div id='status'>Status: READY</div>"

"<div class='info'>"

  "<h2>Game Information</h2>"

  "<p>White Time: <span id='whiteSet'>05:00</span></p>"

  "<p>Black Time: <span id='blackSet'>05:00</span></p>"

  "<p>White Increment: <span id='whiteInc'>0s</span></p>"

  "<p>Black Increment: <span id='blackInc'>0s</span></p>"

  "<p>White Moves: <span id='whiteMoves'>0</span></p>"
 
  "<p>Black Moves: <span id='blackMoves'>0</span></p>"

  "<p>Mode: <span id='mode'>Sudden Death</span></p>"

"</div>"


    "<script>"

    "let ws = new WebSocket('ws://' + window.location.hostname + ':81/');"

    "ws.onopen = function(){"
      "document.getElementById('connection').innerHTML='WebSocket Connected';"
    "};"

    "ws.onclose = function(){"
      "document.getElementById('connection').innerHTML='WebSocket Disconnected';"
    "};"

    "ws.onerror = function(){"
      "document.getElementById('connection').innerHTML='WebSocket Error';"
    "};"

    "ws.onmessage = function(event){"

      "let data = JSON.parse(event.data);"
"let whiteSetMinutes = data.whiteSetMinutes;"
"let whiteSetSeconds = data.whiteSetSeconds;"

"let blackSetMinutes = data.blackSetMinutes;"
"let blackSetSeconds = data.blackSetSeconds;"

"let whiteSetTime ="
  "String(whiteSetMinutes).padStart(2,'0') + ':' +"
  "String(whiteSetSeconds).padStart(2,'0');"

"let blackSetTime ="
  "String(blackSetMinutes).padStart(2,'0') + ':' +"
  "String(blackSetSeconds).padStart(2,'0');"

"document.getElementById('whiteSet').innerHTML = whiteSetTime;"
"document.getElementById('blackSet').innerHTML = blackSetTime;"

"document.getElementById('whiteInc').innerHTML ="
  "data.whiteIncrement + 's';"

"document.getElementById('blackInc').innerHTML ="
  "data.blackIncrement + 's';"

"document.getElementById('whiteMoves').innerHTML ="
"data.whiteMoves;"

"document.getElementById('blackMoves').innerHTML ="
"data.blackMoves;"

"let modeText = 'Sudden Death';"

"if(data.whiteIncrement > 0 || data.blackIncrement > 0){"
  "modeText = 'Fischer Increment';"
"}"

"document.getElementById('mode').innerHTML = modeText;"
      "let whiteMinutes = Math.floor(data.white / 60);"
      "let whiteSeconds = data.white % 60;"

      "let blackMinutes = Math.floor(data.black / 60);"
      "let blackSeconds = data.black % 60;"

      "document.getElementById('whiteTime').innerHTML ="
        "String(whiteMinutes).padStart(2,'0') + ':' +"
        "String(whiteSeconds).padStart(2,'0');"

      "document.getElementById('blackTime').innerHTML ="
        "String(blackMinutes).padStart(2,'0') + ':' +"
        "String(blackSeconds).padStart(2,'0');"

      "let statusText = 'READY';"

      "if(data.state == 8) statusText = 'RUNNING';"
      "else if(data.state == 9) statusText = 'PAUSED';"
      "else if(data.state == 10) statusText = 'GAME OVER';"

      "document.getElementById('status').innerHTML ="
        "'Status: ' + statusText;"

      "document.getElementById('whiteBox').classList.remove('active');"
      "document.getElementById('blackBox').classList.remove('active');"

      "if(data.whiteActive && data.state == 8){"
        "document.getElementById('whiteBox').classList.add('active');"
      "}"

      "if(!data.whiteActive && data.state == 8){"
        "document.getElementById('blackBox').classList.add('active');"
      "}"

    "};"

    "</script>"

    "</body>"
    "</html>"
  );
}
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
 if (type == WStype_CONNECTED) {
  Serial.println("WebSocket client connected");
  sendGameStatus();
}

  if (type == WStype_DISCONNECTED) {
    Serial.println("WebSocket client disconnected");
  }
}
void sendGameStatus() {
  String message = "{";

  message += "\"white\":" + String(whiteTimeLeft) + ",";
  message += "\"black\":" + String(blackTimeLeft) + ",";
  message += "\"whiteActive\":" + String(whiteActive ? "true" : "false") + ",";
  message += "\"state\":" + String((int)gameState) + ",";

  message += "\"whiteSetMinutes\":" + String(whiteSetMinutes) + ",";
  message += "\"whiteSetSeconds\":" + String(whiteSetSeconds) + ",";
  message += "\"blackSetMinutes\":" + String(blackSetMinutes) + ",";
  message += "\"blackSetSeconds\":" + String(blackSetSeconds) + ",";
  message += "\"whiteIncrement\":" + String(whiteIncrement) + ",";
message += "\"blackIncrement\":" + String(blackIncrement) + ",";

message += "\"whiteMoves\":" + String(whiteMoveCount) + ",";
message += "\"blackMoves\":" + String(blackMoveCount);

  message += "}";

  webSocket.broadcastTXT(message);
}
void setup() {
  Serial.begin(115200);
  delay(1000);
    WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();

  Serial.println("Web server started");
  webSocket.begin();
webSocket.onEvent(webSocketEvent);

Serial.println("WebSocket server started");
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

  initRTC();

  lastClkState = digitalRead(ENC_CLK);
  lastTickMillis = millis();

  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
  printGameStatus();
}

// --- RTC init + recovery helpers ---
void initRTC() {
  Wire.begin(RTC_SDA, RTC_SCL);
  Wire.setTimeOut(1000); // ms - avoid permanent freeze on a stuck bus

  rtcAvailable = rtc.begin();

  if (!rtcAvailable) {
    Serial.println("Couldn't find DS3231 - check wiring!");
    return;
  }

  if (rtc.lostPower()) {
    // No coin-cell battery installed yet, so this runs on every boot -
    // it re-syncs to your computer's compile-time clock each upload.
    // Once a CR2032 battery is fitted, this block stops firing and
    // the RTC keeps real time across power cycles on its own.
    Serial.println("RTC lost power - setting time to compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.print("RTC ready. Current time: ");
  printTimestamp(rtc.now());
}

// Toggles SCL manually to unstick a bus a slave is holding low,
// then re-inits Wire and the RTC. Called only if an RTC call fails.
void recoverI2CBus() {
  Wire.end();
  pinMode(RTC_SCL, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(RTC_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(RTC_SCL, LOW);
    delayMicroseconds(5);
  }
  pinMode(RTC_SCL, INPUT);
  initRTC();
}

// Safe wrapper - returns a valid DateTime if the RTC is up,
// otherwise attempts one bus recovery before giving up for this call.
DateTime safeRtcNow() {
  if (!rtcAvailable) {
    recoverI2CBus();
  }
  if (rtcAvailable) {
    return rtc.now();
  }
  return DateTime((uint32_t)0); // fallback - RTC unavailable
}

void printTimestamp(DateTime dt) {
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           dt.year(), dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  Serial.println(buf);
}

void printGameStatus() {
  Serial.println("---- Game Ready ----");
  Serial.print("White: ");
  Serial.print(whiteSetMinutes);
  Serial.print("m ");
  Serial.print(whiteSetSeconds);
  Serial.print("s, increment +");
  Serial.print(whiteIncrement);
  Serial.println("s");

  Serial.print("Black: ");
  Serial.print(blackSetMinutes);
  Serial.print("m ");
  Serial.print(blackSetSeconds);
  Serial.print("s, increment +");
  Serial.print(blackIncrement);
  Serial.println("s");

  Serial.println(
    (whiteIncrement == 0 && blackIncrement == 0)
      ? "Mode: Sudden Death"
      : "Mode: Fischer Increment"
  );
  Serial.println("---------------------");
}

bool wasPressed(Button &btn) {
  bool reading = digitalRead(btn.pin);
  if (reading != btn.lastReading) {
    btn.lastChangeTime = millis();
  }
  bool pressedEvent = false;
  if ((millis() - btn.lastChangeTime) > DEBOUNCE_DELAY) {
    if (reading != btn.stableState) {
      btn.stableState = reading;
      if (btn.stableState == LOW) {
        pressedEvent = true;
      }
    }
  }
  btn.lastReading = reading;
  return pressedEvent;
}

int readEncoderDelta() {
  int clkState = digitalRead(ENC_CLK);
  int delta = 0;
  if (clkState != lastClkState) {
    if (digitalRead(ENC_DT) != clkState) {
      delta = 1;
    } else {
      delta = -1;
    }
  }
  lastClkState = clkState;
  return delta;
}

// --- Blink-aware display helpers for config screens ---
void showMinSecBlink(TM1637Display &disp, int minutes, int seconds, bool blinkMinutesPart, bool blinkOn) {
  uint8_t segs[4];
  bool showMin = blinkMinutesPart ? blinkOn : true;
  bool showSec = blinkMinutesPart ? true : blinkOn;

  segs[0] = showMin ? disp.encodeDigit(minutes / 10) : 0x00;
  segs[1] = showMin ? disp.encodeDigit(minutes % 10) : 0x00;
  segs[2] = showSec ? disp.encodeDigit(seconds / 10) : 0x00;
  segs[3] = showSec ? disp.encodeDigit(seconds % 10) : 0x00;

  segs[1] |= 0x80; // colon always on

  disp.setSegments(segs, 4, 0);
}

void showIncrementBlink(TM1637Display &disp, int value, bool blinkOn) {
  if (blinkOn) {
    disp.showNumberDec(value, false);
  } else {
    uint8_t blank[4] = {0, 0, 0, 0};
    disp.setSegments(blank, 4, 0);
  }
}

void updateSetupDisplay() {
  switch (gameState) {
    case SET_WHITE_MIN:
      showMinSecBlink(displayWhite, whiteSetMinutes, whiteSetSeconds, true, configBlinkOn);
      displayBlack.clear();
      break;
    case SET_WHITE_SEC:
      showMinSecBlink(displayWhite, whiteSetMinutes, whiteSetSeconds, false, configBlinkOn);
      break;
    case SET_BLACK_MIN:
      showMinSecBlink(displayBlack, blackSetMinutes, blackSetSeconds, true, configBlinkOn);
      break;
    case SET_BLACK_SEC:
      showMinSecBlink(displayBlack, blackSetMinutes, blackSetSeconds, false, configBlinkOn);
      break;
    case SET_WHITE_INC:
      showIncrementBlink(displayWhite, whiteIncrement, configBlinkOn);
      break;
    case SET_BLACK_INC:
      showIncrementBlink(displayBlack, blackIncrement, configBlinkOn);
      break;
    default:
      break;
  }
}

void loop() {
  server.handleClient();
  webSocket.loop();
  unsigned long now = millis();

  // --- Config flow ---
  bool inConfig = (gameState == SET_WHITE_MIN || gameState == SET_WHITE_SEC ||
                    gameState == SET_BLACK_MIN || gameState == SET_BLACK_SEC ||
                    gameState == SET_WHITE_INC || gameState == SET_BLACK_INC);

  if (inConfig) {
    if (now - lastConfigBlinkMillis >= CONFIG_BLINK_INTERVAL) {
      lastConfigBlinkMillis = now;
      configBlinkOn = !configBlinkOn;
      updateSetupDisplay();
    }

    int delta = readEncoderDelta();
    if (delta != 0) {
      switch (gameState) {
        case SET_WHITE_MIN: whiteSetMinutes = constrain(whiteSetMinutes + delta, 0, 99); break;
        case SET_WHITE_SEC: whiteSetSeconds = constrain(whiteSetSeconds + delta, 0, 59); break;
        case SET_BLACK_MIN: blackSetMinutes = constrain(blackSetMinutes + delta, 0, 99); break;
        case SET_BLACK_SEC: blackSetSeconds = constrain(blackSetSeconds + delta, 0, 59); break;
        case SET_WHITE_INC: whiteIncrement  = constrain(whiteIncrement + delta, 0, 60); break;
        case SET_BLACK_INC: blackIncrement  = constrain(blackIncrement + delta, 0, 60); break;
        default: break;
      }
      updateSetupDisplay();
    }

    if (wasPressed(btnEncSW)) {
      switch (gameState) {
        case SET_WHITE_MIN: gameState = SET_WHITE_SEC; break;
        case SET_WHITE_SEC: gameState = SET_BLACK_MIN; break;
        case SET_BLACK_MIN: gameState = SET_BLACK_SEC; break;
        case SET_BLACK_SEC: gameState = SET_WHITE_INC; break;
        case SET_WHITE_INC: gameState = SET_BLACK_INC; break;
        case SET_BLACK_INC:
          whiteTimeLeft = whiteSetMinutes * 60L + whiteSetSeconds;
          blackTimeLeft = blackSetMinutes * 60L + blackSetSeconds;
          gameState = READY;
          showTime(displayWhite, whiteTimeLeft);
          showTime(displayBlack, blackTimeLeft);
          printGameStatus();
          break;
        default: break;
      }
      configBlinkOn = true;
      lastConfigBlinkMillis = now;
      updateSetupDisplay();
    }
    return;
  }

  // --- Game over: freeze everything except reset ---
  if (gameState == GAME_OVER) {
    if (wasPressed(btnReset)) {
      resetToDefault();
    }
    return;
  }

  // --- Encoder push enters config, only when safe ---
  if (wasPressed(btnEncSW)) {
    if (gameState == READY || gameState == PAUSED) {
      gameState = SET_WHITE_MIN;
      configBlinkOn = true;
      lastConfigBlinkMillis = now;
      updateSetupDisplay();
      return;
    }
  }

  // --- Normal game buttons ---
  if (wasPressed(btnStart)) {
    if (gameState == READY || gameState == PAUSED) {
      gameState = RUNNING;
      showTime(displayWhite, whiteTimeLeft);
      showTime(displayBlack, blackTimeLeft);

      if (!gameStartLogged) {
        Serial.print("GAME STARTED at: ");
        printTimestamp(safeRtcNow());
        gameStartLogged = true;
      }
      sendGameStatus();
    }
  }

  if (wasPressed(btnPause)) {
    if (gameState == RUNNING) {
      gameState = PAUSED;
      digitalWrite(LED_WHITE_PIN, LOW);
      digitalWrite(LED_BLACK_PIN, LOW);

      sendGameStatus();
    }
  }

  if (wasPressed(btnReset)) {
    if (gameStartLogged) {
      Serial.print("GAME ENDED at: ");
      printTimestamp(safeRtcNow());
    }
    resetToDefault();
  }

  if (wasPressed(btnWhite)) {
    if (gameState == RUNNING && whiteActive) {
      whiteTimeLeft += whiteIncrement;
      whiteMoveCount++;
      whiteActive = false;
      showTime(displayWhite, whiteTimeLeft);

      sendGameStatus();

      Serial.print("WHITE moved at: ");
      printTimestamp(safeRtcNow());
      Serial.print("  White time left: ");
      Serial.println(whiteTimeLeft);

       Serial.print("White moves: ");
    Serial.println(whiteMoveCount);
    }
  }

  if (wasPressed(btnBlack)) {
    if (gameState == RUNNING && !whiteActive) {
      blackTimeLeft += blackIncrement;
      blackMoveCount++;
      whiteActive = true;
      showTime(displayBlack, blackTimeLeft);

      sendGameStatus();

      Serial.print("BLACK moved at: ");
      printTimestamp(safeRtcNow());
      Serial.print("  Black time left: ");
      Serial.println(blackTimeLeft);

       Serial.print("Black moves: ");
    Serial.println(blackMoveCount);
    }
  }

  // --- Clock ticking (only place that runs once per second) ---
  if (gameState == RUNNING && (now - lastTickMillis >= 1000)) {
    lastTickMillis = now;
    if (whiteActive && whiteTimeLeft > 0) {
      whiteTimeLeft--;
    } else if (!whiteActive && blackTimeLeft > 0) {
      blackTimeLeft--;
    }

    showTime(displayWhite, whiteTimeLeft);
    showTime(displayBlack, blackTimeLeft);

    sendGameStatus();

    if (whiteTimeLeft <= 0 || blackTimeLeft <= 0) {
      whiteTimeLeft = max(whiteTimeLeft, 0L);
      blackTimeLeft = max(blackTimeLeft, 0L);
      gameState = GAME_OVER;
      digitalWrite(LED_WHITE_PIN, LOW);
      digitalWrite(LED_BLACK_PIN, LOW);
      digitalWrite(LED_ALERT_PIN, HIGH);

      Serial.print("GAME OVER (time out) at: ");
      printTimestamp(safeRtcNow());
      sendGameStatus();
    }
  } else if (gameState != RUNNING) {
    lastTickMillis = now;
  }

  // --- Active-player LEDs + low-time blink ---
  if (gameState == RUNNING) {
    long activeTimeLeft = whiteActive ? whiteTimeLeft : blackTimeLeft;
    bool lowTime = (activeTimeLeft <= LOW_TIME_THRESHOLD);

    if (lowTime) {
      if (now - lastBlinkMillis >= 250) {
        lastBlinkMillis = now;
        blinkState = !blinkState;
      }
      digitalWrite(LED_WHITE_PIN, (whiteActive && blinkState) ? HIGH : LOW);
      digitalWrite(LED_BLACK_PIN, (!whiteActive && blinkState) ? HIGH : LOW);
      digitalWrite(LED_ALERT_PIN, blinkState ? HIGH : LOW);
    } else {
      digitalWrite(LED_WHITE_PIN, whiteActive ? HIGH : LOW);
      digitalWrite(LED_BLACK_PIN, !whiteActive ? HIGH : LOW);
      digitalWrite(LED_ALERT_PIN, LOW);
    }
  }
}

void resetToDefault() {
  gameStartLogged = false;

  whiteSetMinutes = 5; whiteSetSeconds = 0;
  blackSetMinutes = 5; blackSetSeconds = 0;
  whiteIncrement=0 ; blackIncrement=0;

  whiteTimeLeft = 5L * 60;
  blackTimeLeft = 5L * 60;
  whiteActive = true;

   whiteMoveCount = 0;
   blackMoveCount = 0;
  gameState = READY;

  digitalWrite(LED_WHITE_PIN, LOW);
  digitalWrite(LED_BLACK_PIN, LOW);
  digitalWrite(LED_ALERT_PIN, LOW);
  lastBlinkMillis = 0;
  blinkState = false;

  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
  printGameStatus();
  
}

void showTime(TM1637Display &disp, long secondsLeft) {
  int minutes = secondsLeft / 60;
  int seconds = secondsLeft % 60;
  int value = minutes * 100 + seconds;
  disp.showNumberDecEx(value, 0b11100000, true);
}