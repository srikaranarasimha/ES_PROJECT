# ES_PROJECT

## Chess clock prototype

The ESP32 sketch is in `sketch_sep18b/`. It supports two countdown clocks,
per-player Fischer increments, physical button/encoder setup, pause/resume,
LED indicators, optional DS3231 timestamps, and a live read-only web dashboard.
The clock continues to run if Wi-Fi is not configured or unavailable.

### Build and local dashboard

Open `sketch_sep18b/sketch_sep18b.ino` in the Arduino IDE with an ESP32 board
selected. Install the `TM1637Display`, `RTClib`, and `WebSockets` libraries.
Wi-Fi is optional. To enable the dashboard, copy
`sketch_sep18b/secrets.h.example` to `sketch_sep18b/secrets.h` and enter the
network name and password there. If `secrets.h` already exists, keep it and
merge the needed `#define` lines instead of replacing it. The local credentials
file is ignored by Git. Once connected, open the ESP32's IP address shown in
the Serial Monitor.

The sketch retains the prototype's GPIO assignments; check the wiring before
uploading. The dashboard displays live clock and move-thinking statistics but
does not control the clock remotely.

### Public GitHub Pages dashboard

The static dashboard in `docs/` reads the most recent snapshot from Adafruit IO.
The feed is intentionally public and read-only from the dashboard, so anyone
can view its game status and history. Do not put the Adafruit IO key in
`docs/config.js` or any other public file.

1. In Adafruit IO, create a feed with the key `chess-clock-status` and set its
   visibility to **Public**.
2. In your existing ignored `sketch_sep18b/secrets.h`, add the three
   `ADAFRUIT_IO_*` definitions shown in `sketch_sep18b/secrets.h.example`.
   Keep `ADAFRUIT_IO_KEY` private; it is only used by the firmware to publish.
3. Edit `docs/config.js` and set `username` to your public Adafruit IO username.
   The feed key is public and should match the one created above.
4. Push these changes to the `main` branch. In the repository's **Settings >
   Pages**, choose **GitHub Actions** as the build and deployment source. The
   workflow deploys `docs/` on pushes that change the dashboard.
5. After the workflow succeeds, visit
   `https://srikaranarasimha.github.io/ES_PROJECT/`.

The ESP32 publishes the latest status about every 3 seconds (about 20 data
points per minute). HTTPS publishing runs on a separate low-priority task, and
the clock continues locally if Wi-Fi or Adafruit IO is unavailable. The ESP32
supports 2.4 GHz Wi-Fi only; a 5 GHz-only college network will not connect.

### Power management

- The ESP32 runs at 80 MHz, uses Wi-Fi modem sleep, and dims the displays when
  the clock is not running.
- After 2 minutes without physical input in READY or PAUSED, it disconnects
  Wi-Fi and enters light sleep. Press the Start button (GPIO 27) to wake; that
  press only wakes the device. Press Start again to start or resume. The
  dashboard is unavailable during sleep and reconnects afterward.
- If still idle at 5 minutes, it saves the selected time control and READY or
  PAUSED game state to NVS, then enters deep sleep. Press Start to wake, then
  press it again to start or resume. A paused game can then be resumed.
- The running clock never enters sleep. Configuration and GAME OVER also stay
  awake.

GPIO 15 is assigned to a single 300 ms timeout beep. It is an ESP32 strapping
pin, so use a small 3.3 V passive piezo and do not add a strong pull-up or
pull-down to it. A 5 V or active buzzer needs a transistor driver and wiring
that does not disturb boot strapping.

Deep sleep restarts the ESP32 and turns off its Wi-Fi. A USB power bank may
shut itself off at the resulting low load; test your specific power source.
The Start button must pull GPIO 27 to ground when pressed for sleep wake-up.
The displays are blanked, but their supply is not physically switched off.
Further savings require suitable display power switching and compatible
hardware. Verify the battery output is regulated 5 V and use the board's
documented power input; never connect a bare cell directly to a 5 V or 3.3 V
pin.

MicroSD game logging, Bronstein delay, battery-voltage monitoring, watchdog
handling, and automatic board sensing are not implemented in this prototype.
