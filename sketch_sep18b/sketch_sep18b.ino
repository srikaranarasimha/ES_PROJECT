#include <TM1637Display.h>
#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

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
// BTN_SPARE (GPIO4) removed - that pin belongs to the RTC (SCL)

// Rotary encoder
#define ENC_CLK 17
#define ENC_DT  16
#define ENC_SW  5

// LEDs
#define LED_WHITE_PIN  22
#define LED_BLACK_PIN  21
#define LED_ALERT_PIN  2

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

void setup() {
  Serial.begin(115200);
  delay(1000);
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

  // --- RTC setup (SDA=23, SCL=4 - avoids strapping pins 12/15) ---
  Wire.begin(23, 4);
  Wire.setTimeOut(1000);

  if (!rtc.begin()) {
    Serial.println("Couldn't find DS3231 - check wiring!");
  } else {
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting time to compile time.");
     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    Serial.print("RTC ready. Current time: ");
    printTimestamp(rtc.now());
  }

  lastClkState = digitalRead(ENC_CLK);
  lastTickMillis = millis();

  Serial.println("Chess clock ready - 5:00 / 5:00, no increment");
  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
}

void printTimestamp(DateTime dt) {
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           dt.year(), dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  Serial.println(buf);
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
        printTimestamp(rtc.now());
        gameStartLogged = true;
      }
    }
  }

  if (wasPressed(btnPause)) {
    if (gameState == RUNNING) {
      gameState = PAUSED;
      digitalWrite(LED_WHITE_PIN, LOW);
      digitalWrite(LED_BLACK_PIN, LOW);
    }
  }

  if (wasPressed(btnReset)) {
    if (gameStartLogged) {
      Serial.print("GAME ENDED at: ");
      printTimestamp(rtc.now());
    }
    resetToDefault();
  }

  if (wasPressed(btnWhite)) {
    if (gameState == RUNNING && whiteActive) {
      whiteTimeLeft += whiteIncrement;
      whiteActive = false;
      showTime(displayWhite, whiteTimeLeft);

      Serial.print("WHITE moved at: ");
      printTimestamp(rtc.now());
      Serial.print("  White time left: ");
      Serial.println(whiteTimeLeft);
    }
  }

  if (wasPressed(btnBlack)) {
    if (gameState == RUNNING && !whiteActive) {
      blackTimeLeft += blackIncrement;
      whiteActive = true;
      showTime(displayBlack, blackTimeLeft);

      Serial.print("BLACK moved at: ");
      printTimestamp(rtc.now());
      Serial.print("  Black time left: ");
      Serial.println(blackTimeLeft);
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

    if (whiteTimeLeft <= 0 || blackTimeLeft <= 0) {
      whiteTimeLeft = max(whiteTimeLeft, 0L);
      blackTimeLeft = max(blackTimeLeft, 0L);
      gameState = GAME_OVER;
      digitalWrite(LED_WHITE_PIN, LOW);
      digitalWrite(LED_BLACK_PIN, LOW);
      digitalWrite(LED_ALERT_PIN, HIGH);

      Serial.print("GAME OVER (time out) at: ");
      printTimestamp(rtc.now());
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
  whiteTimeLeft = 5L * 60;
  blackTimeLeft = 5L * 60;
  whiteActive = true;
  gameState = READY;

  digitalWrite(LED_WHITE_PIN, LOW);
  digitalWrite(LED_BLACK_PIN, LOW);
  digitalWrite(LED_ALERT_PIN, LOW);
  lastBlinkMillis = 0;
  blinkState = false;

  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
}

void showTime(TM1637Display &disp, long secondsLeft) {
  int minutes = secondsLeft / 60;
  int seconds = secondsLeft % 60;
  int value = minutes * 100 + seconds;
  disp.showNumberDecEx(value, 0b11100000, true);
}