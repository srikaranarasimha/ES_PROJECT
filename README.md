# ES_PROJECT

## Chess clock prototype

The ESP32 sketch is in `sketch_sep18b/`. It supports two countdown clocks,
per-player Fischer increments, physical button/encoder setup, pause/resume,
LED indicators, optional DS3231 timestamps, and a live read-only web dashboard.
The clock continues to run if Wi-Fi is not configured or unavailable.

### Build and Wi-Fi setup

Open `sketch_sep18b/sketch_sep18b.ino` in the Arduino IDE with an ESP32 board
selected. Install the `TM1637Display`, `RTClib`, and `WebSockets` libraries.
Wi-Fi is optional. To enable the dashboard, copy
`sketch_sep18b/secrets.h.example` to `sketch_sep18b/secrets.h` and enter the
network name and password there. The local credentials file is ignored by Git.
Once connected, open the ESP32's IP address shown in the Serial Monitor.

The sketch retains the prototype's GPIO assignments; check the wiring before
uploading. The dashboard displays live clock and move-thinking statistics but
does not control the clock remotely.

MicroSD game logging, Bronstein delay, persistent settings, battery management,
watchdog handling, buzzer alerts, and automatic board sensing are not
implemented in this prototype.
