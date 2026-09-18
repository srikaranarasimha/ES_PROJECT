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
  SET_WHITE_TIME,
  SET_WHITE_SECONDS,
  SET_BLACK_TIME,
  SET_BLACK_SECONDS,
  SET_INCREMENT,
  READY,
  RUNNING,
  PAUSED
};

GameState gameState = READY;

// Setup values
int whiteSetMinutes = 5;
int whiteSetSeconds = 0;

int blackSetMinutes = 5;
int blackSetSeconds = 0;

int incrementSeconds = 0;

// Actual running clocks
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

      displayWhite.showNumberDecEx(
        whiteSetMinutes * 100,
        0b11100000,
        true
      );

      displayBlack.clear();

      Serial.print("Set White minutes: ");
      Serial.println(whiteSetMinutes);

      break;


    case SET_WHITE_SECONDS:

      displayWhite.showNumberDecEx(
        whiteSetMinutes * 100 + whiteSetSeconds,
        0b11100000,
        true
      );

      displayBlack.clear();

      Serial.print("Set White seconds: ");
      Serial.println(whiteSetSeconds);

      break;


    case SET_BLACK_TIME:

      displayWhite.showNumberDecEx(
        whiteSetMinutes * 100 + whiteSetSeconds,
        0b11100000,
        true
      );

      displayBlack.showNumberDecEx(
        blackSetMinutes * 100,
        0b11100000,
        true
      );

      Serial.print("Set Black minutes: ");
      Serial.println(blackSetMinutes);

      break;


    case SET_BLACK_SECONDS:

      displayWhite.showNumberDecEx(
        whiteSetMinutes * 100 + whiteSetSeconds,
        0b11100000,
        true
      );

      displayBlack.showNumberDecEx(
        blackSetMinutes * 100 + blackSetSeconds,
        0b11100000,
        true
      );

      Serial.print("Set Black seconds: ");
      Serial.println(blackSetSeconds);

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

  // =================================================
  // CONFIGURATION MODE
  // =================================================

  if (gameState == SET_WHITE_TIME ||
      gameState == SET_WHITE_SECONDS ||
      gameState == SET_BLACK_TIME ||
      gameState == SET_BLACK_SECONDS ||
      gameState == SET_INCREMENT) {

    int delta = readEncoderDelta();

    // Encoder rotation
    if (delta != 0) {

      if (gameState == SET_WHITE_TIME) {

        whiteSetMinutes = constrain(
          whiteSetMinutes + delta,
          1,
          99
        );

      } else if (gameState == SET_WHITE_SECONDS) {

        whiteSetSeconds = constrain(
          whiteSetSeconds + delta,
          0,
          59
        );

      } else if (gameState == SET_BLACK_TIME) {

        blackSetMinutes = constrain(
          blackSetMinutes + delta,
          1,
          99
        );

      } else if (gameState == SET_BLACK_SECONDS) {

        blackSetSeconds = constrain(
          blackSetSeconds + delta,
          0,
          59
        );

      } else if (gameState == SET_INCREMENT) {

        incrementSeconds = constrain(
          incrementSeconds + delta,
          0,
          60
        );
      }

      updateSetupDisplay();
    }

    // Encoder button = confirm current setting
    if (wasPressed(btnEncSW)) {

      if (gameState == SET_WHITE_TIME) {

        gameState = SET_WHITE_SECONDS;

      } else if (gameState == SET_WHITE_SECONDS) {

        gameState = SET_BLACK_TIME;

      } else if (gameState == SET_BLACK_TIME) {

        gameState = SET_BLACK_SECONDS;

      } else if (gameState == SET_BLACK_SECONDS) {

        gameState = SET_INCREMENT;

      } else if (gameState == SET_INCREMENT) {

        // Convert configured time into seconds
        whiteTimeLeft =
          whiteSetMinutes * 60L +
          whiteSetSeconds;

        blackTimeLeft =
          blackSetMinutes * 60L +
          blackSetSeconds;

        whiteActive = true;

        gameState = READY;

        Serial.println("Configuration saved.");

        Serial.print("White: ");
        Serial.print(whiteSetMinutes);
        Serial.print(":");
        if (whiteSetSeconds < 10) Serial.print("0");
        Serial.println(whiteSetSeconds);

        Serial.print("Black: ");
        Serial.print(blackSetMinutes);
        Serial.print(":");
        if (blackSetSeconds < 10) Serial.print("0");
        Serial.println(blackSetSeconds);

        Serial.print("Increment: ");
        Serial.print(incrementSeconds);
        Serial.println(" seconds");

        Serial.print("Mode: ");

        if (incrementSeconds == 0) {
          Serial.println("Sudden Death");
        } else {
          Serial.println("Fischer Increment");
        }

        showTime(displayWhite, whiteTimeLeft);
        showTime(displayBlack, blackTimeLeft);

        return;
      }

      updateSetupDisplay();
    }

    return;
  }


  // =================================================
  // ENTER CONFIGURATION
  // =================================================

  if (wasPressed(btnEncSW)) {

    if (gameState == READY || gameState == PAUSED) {

      gameState = SET_WHITE_TIME;

      Serial.println("Entering time configuration");

      updateSetupDisplay();

      return;
    }
  }


  // =================================================
  // START
  // =================================================

  if (wasPressed(btnStart)) {

    if (gameState == READY || gameState == PAUSED) {

      gameState = RUNNING;

      lastTickMillis = millis();

      Serial.println("Game RUNNING");
    }
  }


  // =================================================
  // PAUSE
  // =================================================

  if (wasPressed(btnPause)) {

    if (gameState == RUNNING) {

      gameState = PAUSED;

      Serial.println("Game PAUSED");
    }
  }


  // =================================================
  // RESET
  // =================================================

  if (wasPressed(btnReset)) {

    whiteTimeLeft =
      whiteSetMinutes * 60L +
      whiteSetSeconds;

    blackTimeLeft =
      blackSetMinutes * 60L +
      blackSetSeconds;

    whiteActive = true;

    gameState = READY;

    Serial.println("Reset - configured time restored");

    showTime(displayWhite, whiteTimeLeft);
    showTime(displayBlack, blackTimeLeft);
  }


  // =================================================
  // WHITE BUTTON
  // =================================================

  if (wasPressed(btnWhite)) {

    if (gameState == RUNNING && whiteActive) {

      // Fischer increment
      whiteTimeLeft += incrementSeconds;

      whiteActive = false;

      lastTickMillis = millis();

      Serial.println(
        "White pressed - Black's clock now running"
      );
    }
  }


  // =================================================
  // BLACK BUTTON
  // =================================================

  if (wasPressed(btnBlack)) {

    if (gameState == RUNNING && !whiteActive) {

      // Fischer increment
      blackTimeLeft += incrementSeconds;

      whiteActive = true;

      lastTickMillis = millis();

      Serial.println(
        "Black pressed - White's clock now running"
      );
    }
  }


  // =================================================
  // CLOCK TICKING
  // =================================================

  if (gameState == RUNNING &&
      (now - lastTickMillis >= 1000)) {

    lastTickMillis += 1000;

    if (whiteActive && whiteTimeLeft > 0) {

      whiteTimeLeft--;

    } else if (!whiteActive && blackTimeLeft > 0) {

      blackTimeLeft--;
    }

  } else if (gameState != RUNNING) {

    lastTickMillis = now;
  }


  // =================================================
  // UPDATE DISPLAYS
  // =================================================

  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
}


// =====================================================
// SHOW TIME
// =====================================================

void showTime(
  TM1637Display &disp,
  long secondsLeft
) {

  int minutes = secondsLeft / 60;
  int seconds = secondsLeft % 60;

  int value = minutes * 100 + seconds;

  disp.showNumberDecEx(
    value,
    0b11100000,
    true
  );
}
