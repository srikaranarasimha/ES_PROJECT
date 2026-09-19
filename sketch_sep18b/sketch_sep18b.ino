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
#define BTN_SPARE   4

// Rotary encoder
#define ENC_CLK 17
#define ENC_DT  16
#define ENC_SW  5

// LEDs (no buzzer yet - added in a later pass)
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

void setup() {
  Serial.begin(115200);
  displayWhite.setBrightness(0x0f);
  displayBlack.setBrightness(0x0f);

  pinMode(BTN_WHITE, INPUT_PULLUP);
  pinMode(BTN_BLACK, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_PAUSE, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BTN_SPARE, INPUT_PULLUP);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  pinMode(LED_WHITE_PIN, OUTPUT);
  pinMode(LED_BLACK_PIN, OUTPUT);
  pinMode(LED_ALERT_PIN, OUTPUT);
  digitalWrite(LED_WHITE_PIN, LOW);
  digitalWrite(LED_BLACK_PIN, LOW);
  digitalWrite(LED_ALERT_PIN, LOW);

Wire.begin(23, 4); // SDA=23, SCL=4 - avoids all strapping pins
  if (!rtc.begin()) {
    Serial.println("Couldn't find DS3231 - check wiring!");
  } else {
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting time to compile time.");
      // After first successful flash, comment the line below out and
      // re-upload - otherwise it keeps resetting to compile-time.
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

void updateSetupDisplay() {
  switch (gameState) {
    case SET_WHITE_MIN:
      displayWhite.showNumberDecEx(whiteSetMinutes * 100, 0b11100000, true);
      displayBlack.clear();
      break;
    case SET_WHITE_SEC:
      displayWhite.showNumberDecEx(whiteSetMinutes * 100 + whiteSetSeconds, 0b11100000, true);
      break;
    case SET_BLACK_MIN:
      displayBlack.showNumberDecEx(blackSetMinutes * 100, 0b11100000, true);
      break;
    case SET_BLACK_SEC:
      displayBlack.showNumberDecEx(blackSetMinutes * 100 + blackSetSeconds, 0b11100000, true);
      break;
    case SET_WHITE_INC:
      displayWhite.showNumberDec(whiteIncrement, false);
      break;
    case SET_BLACK_INC:
      displayBlack.showNumberDec(blackIncrement, false);
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
      updateSetupDisplay();
      return;
    }
  }

  // --- Normal game buttons ---
  if (wasPressed(btnStart)) {
    if (gameState == READY || gameState == PAUSED) {
      gameState = RUNNING;

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

      Serial.print("BLACK moved at: ");
      printTimestamp(rtc.now());
      Serial.print("  Black time left: ");
      Serial.println(blackTimeLeft);
    }
  }

  // --- Clock ticking ---
  if (gameState == RUNNING && (now - lastTickMillis >= 1000)) {
    lastTickMillis = now;
    if (whiteActive && whiteTimeLeft > 0) {
      whiteTimeLeft--;
    } else if (!whiteActive && blackTimeLeft > 0) {
      blackTimeLeft--;
    }

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

  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
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