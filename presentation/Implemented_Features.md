# Implemented features — ESP32 chess clock

This inventory describes what is present in the current `sketch_sep18b.ino`
firmware and the current dashboard files. Items from the project report that
are not implemented are listed separately to avoid presenting plans as working
features.

## Clock and time control

- Independent White and Black countdown clocks, each shown on a four-digit
  TM1637 display.
- Millisecond elapsed-time accounting with `millis()` rather than a
  one-second blocking delay. The running clock is not suspended for Wi-Fi
  connectivity.
- Configurable White and Black starting times: 0–99 minutes and 0–59 seconds.
- Per-player Fischer increment, configured from 0–60 seconds and added to that
  player's clock when their move button is pressed.
- Default time control is 5:00 per player with no increment.
- Rotary-encoder configuration sequence for White minutes/seconds, Black
  minutes/seconds, then White/Black increments. The selected field blinks
  during setup.
- Start, pause, resume, and reset controls. White/Black move buttons are
  accepted only for the side whose turn is active.
- READY, RUNNING, PAUSED, and GAME OVER states, plus setup states.
- Button debouncing (35 ms) and pull-up input configuration.
- Start is rejected if either player's remaining time is zero.
- At timeout, the active player's time is clamped and displayed at exactly
  zero; the result identifies which player flagged and who wins.

## Physical feedback and analytics

- White/Black LEDs indicate the active player.
- At 30 seconds or less on the active clock, the LEDs flash to signal time
  pressure.
- A passive piezo produces one 300 ms, 2400 Hz timeout beep on GPIO 15. GPIO 15
  is an ESP32 strapping pin; wiring must not pull it strongly high or low.
- Move count, total thinking time, average thinking time per move, and longest
  and shortest move time are tracked separately for each player.
- Paused thinking time is carried into the turn's accumulated thinking time.

## RTC and saved settings

- Optional DS3231 RTC connected over I2C (SDA GPIO 23, SCL GPIO 4).
- Game-start and game-over events are timestamped to Serial when the RTC is
  available. If the RTC has lost power, firmware sets it to the sketch compile
  time; without an RTC, game timing still works.
- ESP32 Preferences/NVS stores clock settings across regular reboots.
- Before deep sleep, firmware saves an idle READY/PAUSED game snapshot,
  including clock values, active side, move statistics, and pause timing.
  Valid snapshots are restored on wake.

## Wi-Fi dashboards and cloud telemetry

- Optional 2.4 GHz Wi-Fi; the physical clock continues operating if Wi-Fi is
  absent or disconnected. The ESP32 does not support a 5 GHz-only network.
- Local dashboard served by the ESP32 over HTTP, with live status delivered
  over WebSockets. The local dashboard is read-only and does not control the
  clock.
- Dashboard status includes both clocks, active side/state/result, starting
  times, increments/time-control label, RTC availability, move counts, and
  thinking-time analytics.
- Optional Adafruit IO publishing to a configured feed about every 3 seconds
  (approximately 20 writes per minute). TLS certificate verification is used;
  the private Adafruit key belongs only in the ignored local `secrets.h`.
- Cloud publishing uses a one-slot FreeRTOS queue and a separate low-priority
  worker task so HTTPS requests do not run in the clock loop.
- A separate static GitHub Pages dashboard reads the public Adafruit IO feed
  every second and interpolates the displayed countdown between snapshots.
  The feed must be public for the keyless public dashboard; anyone can see its
  published game data.

## Power and recovery

- CPU frequency is set to 80 MHz; Wi-Fi modem sleep is enabled.
- Display brightness is reduced when the clock is not running.
- In READY or PAUSED, two minutes of inactivity triggers light sleep and turns
  Wi-Fi off. Start (GPIO 27) wakes the board; the wake press is consumed and
  does not start/resume the game.
- At five minutes of inactivity, firmware saves state to NVS and enters deep
  sleep. Start wakes the ESP32; a separate press starts/resumes the clock.
- A running clock is never put into idle sleep. Setup and GAME OVER also stay
  awake.
- The ESP32 task watchdog monitors the main Arduino loop with a five-second
  timeout and panic reset. It is disabled around intentional light sleep and
  re-enabled after wake.

## Pin assignments

| Component | Firmware pins |
| --- | --- |
| White TM1637 display | CLK 18, DIO 19 |
| Black TM1637 display | CLK 25, DIO 26 |
| White / Black move buttons | GPIO 32 / 33 |
| Start / Pause / Reset buttons | GPIO 27 / 14 / 13 |
| Encoder A / B / switch | GPIO 17 / 16 / 5 |
| White / Black / alert LEDs | GPIO 22 / 21 / 2 |
| Piezo buzzer | GPIO 15 |
| DS3231 RTC | SDA 23, SCL 4 |

Use 3.3 V-compatible signals for ESP32 GPIO and a regulated supply appropriate
for the DevKit. GPIO 2, 5, and 15 are strapping pins on the classic ESP32;
check that attached LEDs, the encoder switch, and buzzer do not interfere with
boot strapping. The TM1637 interface is a dedicated two-wire protocol, not
standard I2C; the DS3231 is the I2C device in this implementation.

## Not implemented in the current prototype

- MicroSD/SPI game-history logging.
- Bronstein delay; only Fischer increment is implemented.
- Battery-voltage measurement or battery-percentage display.
- MCP23017 I/O expansion.
- Chessboard occupancy sensing (reed switches/Hall sensors) or automatic move
  detection.
- Remote dashboard controls.
- Persistent historical game records beyond the saved settings/temporary
  deep-sleep snapshot.
- Physical power switching for the displays or battery charging circuitry.

## Embedded-systems implementation detail

### Real-time clock timing

The countdown is updated through `millis()` elapsed-time subtraction rather than
by decrementing a counter inside a blocking `delay(1000)` loop. The elapsed
millisecond value is subtracted from the active player's remaining time on
every loop pass. Displaying `minutes:seconds` uses ceiling-based seconds-left
conversion so a partial second does not appear as zero before the actual
timeout. At timeout the active time is clamped to zero and the game transitions
to `GAME_OVER`.

This architecture keeps timing independent from Wi-Fi HTTP handling and avoids
using network round trips as the game clock. The ESP32's `millis()` source does
not provide perfect external clock accuracy but is suitable for this non-safety-
critical software clock. A future real-time hardware timer could provide tighter
interrupt-time accuracy.

### TM1637 and I2C interfaces

TM1637 is a proprietary two-wire display protocol. It is separate from standard
I2C and is configured as one TM1637 instance per player. The two displays use
the dedicated GPIO pairs 18/19 and 25/26. They are used for readable numerical
display only; no TM1637 timing is part of the game-state timing calculation.

The DS3231 RTC uses the ESP32 I2C master implementation on SDA GPIO 23 and SCL
GPIO 4. `Wire.setTimeOut(1000)` prevents an absent or faulty RTC from blocking
normal setup indefinitely. RTC availability is tested at startup and is also
reported in the local/cloud status payload. An unavailable RTC reduces only
timestamp functionality; it does not stop the countdown.

### GPIO, debounce, and alerts

Buttons and encoder inputs use internal pull-ups and are interpreted as
active-low. `wasPressed()` implements a 35 ms stable-state debounce. The
physical move button is accepted only when the corresponding side is active.
Start and reset have separate state-guarded paths.

GPIO 22 and 21 drive the White and Black indicators. GPIO 2 is an alert LED.
At 30 seconds or fewer for the active player, the indicators flash in a
250 ms update cycle. GPIO 15 drives the piezo at 2400 Hz for 300 ms. GPIO 15
is an ESP32 strapping pin, so boot-related pull resistors must be avoided or
carefully designed.

### FreeRTOS concurrency

The Arduino `loop()` executes as a FreeRTOS task. Network handling is not placed
in this real-time path: a 768-byte `TelemetrySnapshot` queue has a depth of one.
The main loop uses `xQueueOverwrite()` to retain only the newest JSON snapshot,
avoiding backlog growth. A separate low-priority FreeRTOS task receives each
snapshot and performs Wi-Fi time synchronization, TLS setup, and the HTTPS POST.

This separates slow, potentially blocking operations from `updateClock()`,
button handling, display refresh, and timeout processing. The queue prevents
one network failure from coupling the cloud transport directly to the chess
clock state.

### NVS and state retention

ESP32 Preferences/NVS stores White/Black minutes and seconds and both Fischer
increments in the `chessclk` namespace. Deep-sleep storage adds a magic value,
version, game state, active side, remaining clock values, pause accumulator,
RTC/game-start marker, and both player-stat structures.

Restoration is intentionally guarded: the snapshot marker must exist, the
stored structure must have the exact size, the magic and version must match,
the state must be READY or PAUSED, and clock values must be non-negative.
Running state is not restored after deep sleep because deep-sleep snapshots are
only saved for idle READY/PAUSED states.

### Wi-Fi, HTTP, WebSockets, HTTPS, and TLS

Wi-Fi station mode initializes in the background and does not block setup. The
local WebServer listens on port 80 and the WebSockets server listens on port 81.
Local status is JSON-encoded and broadcast to connected clients. The browser
does not send control commands and cannot modify the clock.

Cloud publication uses HTTP POST to the Adafruit IO v2 feed endpoint. The
private `X-AIO-Key` is read from ignored local `secrets.h`. The firmware pins a
DigiCert Global Root G2 CA certificate and does not use an insecure TLS fallback.
The Pages site only receives the public feed's latest data record. The HTML and
JavaScript never contain the write key.

### Power management

The CPU is set to 80 MHz. Wi-Fi modem sleep and idle display dimming reduce
active power, and Wi-Fi is disconnected before light sleep. Two idle minutes
in READY/PAUSED enter light sleep, where GPIO 27 wake is enabled. Five idle
minutes enter deep sleep after saving settings and a snapshot.

Running games and configuration states remain awake. Wake-button state is
synchronized after light/deep sleep so the press that wakes the device is not
also interpreted as a Start or Resume command.

The display modules are still powered when blanked; this implementation does
not physically switch their supply. A lower-power final design would use MOSFET
power switching or display-compatible supply control, but that hardware must
protect display start-up and strapping-pin states.

### Watchdog and reset recovery

The ESP32 task watchdog monitors the Arduino loop task with a five-second
timeout. `enableLoopWDT()` subscribes the loop task and the core automatically
feeds it while `loop()` continues. If the loop stalls, the configured panic
watchdog causes an ESP32 reset. The loop watchdog is removed before an
intentional light-sleep call and re-added after wake.

The telemetry task is not the loop's watchdog peer; network delays may affect
publishing but should not stop the clock task. If a full external redundant
watchdog is added later, the ESP32 should feed it with an independent heartbeat
only after both timing and main-loop service remain healthy.
