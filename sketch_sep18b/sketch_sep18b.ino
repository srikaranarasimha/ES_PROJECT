#include <TM1637Display.h>

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

// Game state
enum GameState {
  SET_WHITE_MIN, SET_WHITE_SEC,
  SET_BLACK_MIN, SET_BLACK_SEC,
  SET_WHITE_INC, SET_BLACK_INC,
  READY, RUNNING, PAUSED
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

  lastClkState = digitalRead(ENC_CLK);
  lastTickMillis = millis();

  Serial.println("Chess clock ready - 5:00 / 5:00, no increment");
  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
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
      Serial.print("White minutes: "); Serial.println(whiteSetMinutes);
      break;

    case SET_WHITE_SEC:
      displayWhite.showNumberDecEx(whiteSetMinutes * 100 + whiteSetSeconds, 0b11100000, true);
      Serial.print("White seconds: "); Serial.println(whiteSetSeconds);
      break;

    case SET_BLACK_MIN:
      displayBlack.showNumberDecEx(blackSetMinutes * 100, 0b11100000, true);
      Serial.print("Black minutes: "); Serial.println(blackSetMinutes);
      break;

    case SET_BLACK_SEC:
      displayBlack.showNumberDecEx(blackSetMinutes * 100 + blackSetSeconds, 0b11100000, true);
      Serial.print("Black seconds: "); Serial.println(blackSetSeconds);
      break;

    case SET_WHITE_INC:
      displayWhite.showNumberDec(whiteIncrement, false);
      Serial.print("White increment (sec): "); Serial.println(whiteIncrement);
      break;

    case SET_BLACK_INC:
      displayBlack.showNumberDec(blackIncrement, false);
      Serial.print("Black increment (sec): "); Serial.println(blackIncrement);
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
          Serial.println("Config saved.");
          showTime(displayWhite, whiteTimeLeft);
          showTime(displayBlack, blackTimeLeft);
          break;
        default: break;
      }
      updateSetupDisplay();
    }
    return;
  }

  // --- Encoder push button enters config, only when safe ---
  if (wasPressed(btnEncSW)) {
    if (gameState == READY || gameState == PAUSED) {
      gameState = SET_WHITE_MIN;
      Serial.println("Entering config - set White's minutes");
      updateSetupDisplay();
      return;
    }
  }

  // --- Normal game button handling ---
  if (wasPressed(btnStart)) {
    if (gameState == READY || gameState == PAUSED) {
      gameState = RUNNING;
      Serial.println("Game RUNNING");
    }
  }

  if (wasPressed(btnPause)) {
    if (gameState == RUNNING) {
      gameState = PAUSED;
      Serial.println("Game PAUSED");
    }
  }

  if (wasPressed(btnReset)) {
    whiteSetMinutes = 5; whiteSetSeconds = 0;
    blackSetMinutes = 5; blackSetSeconds = 0;
    whiteTimeLeft = 5L * 60;
    blackTimeLeft = 5L * 60;
    whiteActive = true;
    gameState = READY;
    Serial.println("Reset - both clocks back to 5:00, READY");
    showTime(displayWhite, whiteTimeLeft);
    showTime(displayBlack, blackTimeLeft);
  }

  if (wasPressed(btnWhite)) {
    if (gameState == RUNNING && whiteActive) {
      whiteTimeLeft += whiteIncrement; // each side uses its own increment
      whiteActive = false;
      Serial.println("White pressed - Black's clock now running");
    }
  }

  if (wasPressed(btnBlack)) {
    if (gameState == RUNNING && !whiteActive) {
      blackTimeLeft += blackIncrement;
      whiteActive = true;
      Serial.println("Black pressed - White's clock now running");
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
  } else if (gameState != RUNNING) {
    lastTickMillis = now;
  }

  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
}

void showTime(TM1637Display &disp, long secondsLeft) {
  int minutes = secondsLeft / 60;
  int seconds = secondsLeft % 60;
  int value = minutes * 100 + seconds;
  disp.showNumberDecEx(value, 0b11100000, true);
}