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

// Game state
enum GameState { READY, RUNNING, PAUSED };
GameState gameState = READY;

const long STARTING_TIME = 300; // 5 minutes
long whiteTimeLeft = STARTING_TIME;
long blackTimeLeft = STARTING_TIME;
bool whiteActive = true;

unsigned long lastTickMillis = 0;

// Debounce tracking per button
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

const unsigned long DEBOUNCE_DELAY = 40; // ms

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

  lastTickMillis = millis();
  Serial.println("Phase 3 test starting - buttons wired in");
}

// Returns true exactly once, on the frame a debounced press is detected
bool wasPressed(Button &btn) {
  bool reading = digitalRead(btn.pin);

  if (reading != btn.lastReading) {
    btn.lastChangeTime = millis();
  }

  bool pressedEvent = false;

  if ((millis() - btn.lastChangeTime) > DEBOUNCE_DELAY) {
    if (reading != btn.stableState) {
      btn.stableState = reading;
      if (btn.stableState == LOW) { // active LOW = pressed
        pressedEvent = true;
      }
    }
  }

  btn.lastReading = reading;
  return pressedEvent;
}

void loop() {
  unsigned long now = millis();

  // --- Button handling ---
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
    whiteTimeLeft = STARTING_TIME;
    blackTimeLeft = STARTING_TIME;
    whiteActive = true;
    gameState = READY;
    Serial.println("Game RESET");
  }

  if (wasPressed(btnWhite)) {
    if (gameState == RUNNING && whiteActive) {
      whiteActive = false; // White moved, hand turn to Black
      Serial.println("White pressed - Black's clock now running");
    }
  }

  if (wasPressed(btnBlack)) {
    if (gameState == RUNNING && !whiteActive) {
      whiteActive = true; // Black moved, hand turn to White
      Serial.println("Black pressed - White's clock now running");
    }
  }

  // --- Clock ticking (only while RUNNING) ---
  if (gameState == RUNNING && (now - lastTickMillis >= 1000)) {
    lastTickMillis = now;

    if (whiteActive && whiteTimeLeft > 0) {
      whiteTimeLeft--;
    } else if (!whiteActive && blackTimeLeft > 0) {
      blackTimeLeft--;
    }
  } else if (gameState != RUNNING) {
    lastTickMillis = now; // prevent time jump when resumed
  }

  // --- Display update ---
  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
}

void showTime(TM1637Display &disp, long secondsLeft) {
  int minutes = secondsLeft / 60;
  int seconds = secondsLeft % 60;
  int value = minutes * 100 + seconds;
  disp.showNumberDecEx(value, 0b11100000, true);
}