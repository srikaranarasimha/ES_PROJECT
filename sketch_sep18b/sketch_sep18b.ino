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
enum GameState { SET_WHITE_TIME, SET_BLACK_TIME, SET_INCREMENT, READY, RUNNING, PAUSED };
GameState gameState = READY; // boot directly into READY now

// Setup values (editable via encoder, only when config is entered)
int whiteSetMinutes = 5;
int blackSetMinutes = 5;
int incrementSeconds = 0;

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
    case SET_WHITE_TIME:
      displayWhite.showNumberDecEx(whiteSetMinutes * 100, 0b11100000, true);
      displayBlack.clear();
      Serial.print("Set White minutes: ");
      Serial.println(whiteSetMinutes);
      break;

    case SET_BLACK_TIME:
      displayWhite.showNumberDecEx(whiteSetMinutes * 100, 0b11100000, true);
      displayBlack.showNumberDecEx(blackSetMinutes * 100, 0b11100000, true);
      Serial.print("Set Black minutes: ");
      Serial.println(blackSetMinutes);
      break;

    case SET_INCREMENT:
      displayWhite.showNumberDec(incrementSeconds, false);
      displayBlack.showNumberDec(incrementSeconds, false);
      Serial.print("Set increment (sec): ");
      Serial.println(incrementSeconds);
      break;

    default:
      break;
  }
}

void loop() {
  unsigned long now = millis();

  // --- Config flow (only entered deliberately via encoder press) ---
  if (gameState == SET_WHITE_TIME || gameState == SET_BLACK_TIME || gameState == SET_INCREMENT) {
    int delta = readEncoderDelta();

    if (delta != 0) {
      if (gameState == SET_WHITE_TIME) {
        whiteSetMinutes = constrain(whiteSetMinutes + delta, 1, 99);
      } else if (gameState == SET_BLACK_TIME) {
        blackSetMinutes = constrain(blackSetMinutes + delta, 1, 99);
      } else if (gameState == SET_INCREMENT) {
        incrementSeconds = constrain(incrementSeconds + delta, 0, 60);
      }
      updateSetupDisplay();
    }

    if (wasPressed(btnEncSW)) {
      if (gameState == SET_WHITE_TIME) {
        gameState = SET_BLACK_TIME;
      } else if (gameState == SET_BLACK_TIME) {
        gameState = SET_INCREMENT;
      } else if (gameState == SET_INCREMENT) {
        whiteTimeLeft = whiteSetMinutes * 60L;
        blackTimeLeft = blackSetMinutes * 60L;
        gameState = READY;
        Serial.print("Config saved. Mode: ");
        Serial.println(incrementSeconds == 0 ? "Sudden Death" : "Fischer Increment");
        showTime(displayWhite, whiteTimeLeft);
        showTime(displayBlack, blackTimeLeft);
      }
      updateSetupDisplay();
    }
    return;
  }

  // --- Encoder push button enters config, only when safe to reconfigure ---
  if (wasPressed(btnEncSW)) {
    if (gameState == READY || gameState == PAUSED) {
      gameState = SET_WHITE_TIME;
      Serial.println("Entering time config - set White's minutes");
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
    // Reset always snaps both sides back to a flat 5:00 - no setup screen
    whiteSetMinutes = 5;
    blackSetMinutes = 5;
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
      whiteTimeLeft += incrementSeconds;
      whiteActive = false;
      Serial.println("White pressed - Black's clock now running");
    }
  }

  if (wasPressed(btnBlack)) {
    if (gameState == RUNNING && !whiteActive) {
      blackTimeLeft += incrementSeconds;
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