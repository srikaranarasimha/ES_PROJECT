#include <TM1637Display.h>

// Display 1 - White
#define CLK1 18
#define DIO1 19
TM1637Display displayWhite(CLK1, DIO1);

// Display 2 - Black
#define CLK2 25
#define DIO2 26
TM1637Display displayBlack(CLK2, DIO2);

// Starting time for both clocks: 5 minutes = 300 seconds
long whiteTimeLeft = 300;
long blackTimeLeft = 300;

bool whiteActive = true;  // which player's clock is currently running

unsigned long lastTickMillis = 0;
unsigned long lastSwitchMillis = 0;
const unsigned long switchInterval = 10000; // auto-toggle every 10s, for testing only

void setup() {
  Serial.begin(115200);
  displayWhite.setBrightness(0x0f);
  displayBlack.setBrightness(0x0f);
  lastTickMillis = millis();
  lastSwitchMillis = millis();
  Serial.println("Phase 2 test starting - two independent clocks");
}

void loop() {
  unsigned long now = millis();

  // Tick down once per second, non-blocking
  if (now - lastTickMillis >= 1000) {
    lastTickMillis = now;

    if (whiteActive && whiteTimeLeft > 0) {
      whiteTimeLeft--;
    } else if (!whiteActive && blackTimeLeft > 0) {
      blackTimeLeft--;
    }
  }

  // Auto-toggle active player every 10s (stand-in for a button press)
  if (now - lastSwitchMillis >= switchInterval) {
    lastSwitchMillis = now;
    whiteActive = !whiteActive;
    Serial.print("Switched active player to: ");
    Serial.println(whiteActive ? "White" : "Black");
  }

  // Update both displays every loop (cheap, safe to do often)
  showTime(displayWhite, whiteTimeLeft);
  showTime(displayBlack, blackTimeLeft);
}

void showTime(TM1637Display &disp, long secondsLeft) {
  int minutes = secondsLeft / 60;
  int seconds = secondsLeft % 60;
  int value = minutes * 100 + seconds;
  disp.showNumberDecEx(value, 0b11100000, true); // colon on
}