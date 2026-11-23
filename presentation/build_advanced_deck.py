from pathlib import Path

from PIL import Image
from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE, MSO_CONNECTOR
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.util import Inches, Pt


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "Advanced_Chess_Clock_Project_Presentation.pptx"
DIAGRAM = ROOT / "Circuit_Diagram.png"

BG = "09111F"
PANEL = "111C2E"
PANEL2 = "17263A"
WHITE = "F4F7FB"
MUTED = "A4B1C2"
GREEN = "55D8A6"
BLUE = "66A8FF"
GOLD = "F2C56D"
RED = "FF718A"
CYAN = "64D5E7"
LINE = "2A4058"


def rgb(v):
    return RGBColor.from_string(v)


def add_bg(slide):
    f = slide.background.fill
    f.solid()
    f.fore_color.rgb = rgb(BG)


def box(slide, x, y, w, h, fill=PANEL, line=LINE, radius=True):
    typ = MSO_SHAPE.ROUNDED_RECTANGLE if radius else MSO_SHAPE.RECTANGLE
    s = slide.shapes.add_shape(typ, Inches(x), Inches(y), Inches(w), Inches(h))
    s.fill.solid()
    s.fill.fore_color.rgb = rgb(fill)
    s.line.color.rgb = rgb(line)
    s.line.width = Pt(1)
    return s


def text(slide, value, x, y, w, h, size=16, color=WHITE, bold=False,
         align=PP_ALIGN.LEFT, valign=MSO_ANCHOR.TOP):
    s = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = s.text_frame
    tf.clear()
    tf.word_wrap = True
    tf.margin_left = Inches(0.05)
    tf.margin_right = Inches(0.05)
    tf.margin_top = Inches(0.03)
    tf.margin_bottom = Inches(0.03)
    tf.vertical_anchor = valign
    p = tf.paragraphs[0]
    p.alignment = align
    r = p.add_run()
    r.text = value
    r.font.name = "Aptos"
    r.font.size = Pt(size)
    r.font.bold = bold
    r.font.color.rgb = rgb(color)
    return s


def bullets(slide, values, x, y, w, h, size=14, color=MUTED,
            accent=None, line_gap=7):
    s = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = s.text_frame
    tf.clear()
    tf.word_wrap = True
    tf.margin_left = Inches(0.05)
    tf.margin_right = Inches(0.05)
    tf.margin_top = Inches(0.02)
    for i, value in enumerate(values):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.text = value
        p.font.name = "Aptos"
        p.font.size = Pt(size)
        p.font.color.rgb = rgb(color)
        p.space_after = Pt(line_gap)
        p.level = 0
        p.bullet = True
    return s


def chip(slide, value, x, y, w, color=GREEN):
    box(slide, x, y, w, 0.34, PANEL2, PANEL2)
    text(slide, value, x + 0.03, y + 0.02, w - 0.06, 0.28,
         9, color, True, PP_ALIGN.CENTER, MSO_ANCHOR.MIDDLE)


def card(slide, x, y, w, h, title, body, color=GREEN, body_size=13):
    box(slide, x, y, w, h, PANEL, LINE)
    box(slide, x + 0.03, y + 0.18, 0.055, h - 0.36, color, color, False)
    text(slide, title, x + 0.18, y + 0.16, w - 0.32, 0.38, 16, WHITE, True)
    text(slide, body, x + 0.18, y + 0.64, w - 0.34, h - 0.78,
         body_size, MUTED)


def header(slide, section, title, subtitle, number):
    text(slide, section.upper(), 0.62, 0.24, 3.5, 0.22, 9, GREEN, True)
    text(slide, title, 0.62, 0.58, 12.1, 0.52, 27, WHITE, True)
    text(slide, subtitle, 0.62, 1.10, 12.0, 0.38, 12, MUTED)
    line = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.62),
                                  Inches(7.12), Inches(12.08), Inches(0.012))
    line.fill.solid()
    line.fill.fore_color.rgb = rgb(LINE)
    line.line.fill.background()
    text(slide, "ADVANCED EMBEDDED CHESS-CLOCK SYSTEM",
         0.62, 7.18, 5.5, 0.16, 8, MUTED)
    text(slide, f"{number:02d}", 12.1, 7.16, 0.55, 0.18,
         9, MUTED, True, PP_ALIGN.RIGHT)


def slide(prs, section, title, subtitle, number):
    s = prs.slides.add_slide(prs.slide_layouts[6])
    add_bg(s)
    header(s, section, title, subtitle, number)
    return s


def arrow(slide, x1, y1, x2, y2, color=MUTED, width=2):
    c = slide.shapes.add_connector(MSO_CONNECTOR.STRAIGHT,
                                   Inches(x1), Inches(y1),
                                   Inches(x2), Inches(y2))
    c.line.color.rgb = rgb(color)
    c.line.width = Pt(width)
    c.line.end_arrowhead = True
    return c


prs = Presentation()
prs.slide_width = Inches(13.333)
prs.slide_height = Inches(7.5)

# 1 Cover
s = prs.slides.add_slide(prs.slide_layouts[6])
add_bg(s)
box(s, 7.45, 0.55, 5.25, 6.25, PANEL, PANEL2)
text(s, "EMBEDDED SYSTEMS · ESP32", 0.78, 0.72, 5.8, 0.34,
     11, GREEN, True)
text(s, "ADVANCED DIGITAL\nCHESS CLOCK", 0.78, 1.42, 6.25, 1.55,
     38, WHITE, True)
text(s, "Real-time control · I2C · FreeRTOS · NVS\nVerified networking · Low-power modes · Watchdog",
     0.82, 3.22, 6.15, 0.82, 16, MUTED)
chip(s, "TIMING", 0.82, 4.58, 1.25, GREEN)
chip(s, "RTOS", 2.20, 4.58, 1.25, BLUE)
chip(s, "I2C", 3.58, 4.58, 1.15, GOLD)
chip(s, "POWER", 4.86, 4.58, 1.25, CYAN)
text(s, "ESP32 dual-clock state machine",
     0.82, 5.55, 5.55, 0.32, 13, WHITE, True)
text(s, "Architecture · implementation · validation",
     0.82, 5.95, 5.55, 0.32, 11, MUTED)
text(s, "WHITE", 8.05, 1.28, 2.5, 0.25, 11, MUTED, True)
text(s, "05:00", 7.98, 1.68, 3.4, 0.72, 40, GREEN, True)
text(s, "BLACK", 8.05, 3.15, 2.5, 0.25, 11, MUTED, True)
text(s, "05:00", 7.98, 3.55, 3.4, 0.72, 40, BLUE, True)
text(s, "Illustrative 5:00 + 0 second time control",
     8.05, 5.58, 3.95, 0.32, 10, MUTED)

# 2 Introduction
s = slide(prs, "Introduction", "System concept and engineering scope",
          "A deterministic embedded controller coordinates physical I/O, local UI, cloud telemetry, and recovery.", 2)
card(s, 0.62, 1.72, 3.86, 2.05, "APPLICATION",
     "Independent two-player countdown with configurable Fischer increments, turn switching, timeout handling, and game analytics.",
     GREEN, 14)
card(s, 4.74, 1.72, 3.86, 2.05, "CONTROL PLATFORM",
     "ESP32 classic executes the Arduino main loop and FreeRTOS tasks, while peripherals provide discrete I/O and protocol-based communication.",
     BLUE, 14)
card(s, 8.86, 1.72, 3.86, 2.05, "SYSTEM BOUNDARY",
     "The physical controls and firmware clock remain authoritative. Web dashboards are read-only remote observations.",
     GOLD, 14)
text(s, "END-TO-END DATA FLOW", 0.72, 4.22, 3.5, 0.25, 10, GREEN, True)
nodes = [("PHYSICAL INPUT", GREEN), ("TIME STATE", BLUE),
         ("LOCAL/REMOTE UI", GOLD), ("POWER/RECOVERY", CYAN)]
for i, (label, color) in enumerate(nodes):
    x = 0.74 + i * 3.08
    box(s, x, 4.72, 2.38, 0.92, PANEL2, color)
    text(s, label, x + 0.08, 5.02, 2.22, 0.28, 11, color, True,
         PP_ALIGN.CENTER, MSO_ANCHOR.MIDDLE)
    if i < 3:
        text(s, "→", x + 2.45, 4.98, 0.42, 0.34, 19, MUTED, True,
             PP_ALIGN.CENTER)
text(s, "Requirement: accurate elapsed time must survive network delays, dashboard requests, and reduced CPU frequency.",
     0.82, 6.18, 11.5, 0.48, 13, WHITE)

# 3 Objectives
s = slide(prs, "Objectives", "Functional and embedded-system requirements",
          "The design targets correctness, observability, graceful degradation, and recoverability.", 3)
objectives = [
    ("01", "Time determinism", "Use elapsed wall-time rather than decrement loops or blocking delay().", GREEN),
    ("02", "Protocol integration", "Apply TM1637, I2C/DS3231, HTTP/WebSocket, and verified HTTPS/TLS appropriately.", BLUE),
    ("03", "Concurrent isolation", "Keep HTTPS publishing outside the main clock loop using a FreeRTOS queue/task.", GOLD),
    ("04", "Persistent state", "Store settings and a deep-sleep snapshot in ESP32 NVS.", CYAN),
    ("05", "Fault recovery", "Detect a stalled main loop with a task watchdog and restore operation after reset.", RED),
    ("06", "Energy management", "Reduce CPU/Wi-Fi/display activity and use two sleep levels for idle states.", GREEN),
]
coords = [(0.62, 1.72), (4.74, 1.72), (8.86, 1.72),
          (0.62, 4.02), (4.74, 4.02), (8.86, 4.02)]
for (num, title, body, color), (x, y) in zip(objectives, coords):
    card(s, x, y, 3.86, 1.96, f"{num}  {title}", body, color, 13)

# 4 Specifications
s = slide(prs, "Specifications", "Controller, peripherals, timing, and memory",
          "All values below reflect the firmware source and the ESP32 DevKit target.", 4)
card(s, 0.62, 1.72, 3.86, 2.08, "PROCESSOR",
     "ESP32 classic · 80 MHz\nArduino loop task\nFreeRTOS scheduler\nInput debouncing: 35 ms\nNVS: chessclk namespace",
     BLUE, 13)
card(s, 4.74, 1.72, 3.86, 2.08, "DISCRETE I/O",
     "Two TM1637 displays\nMove/Start/Pause/Reset buttons\nRotary encoder\n3 status LEDs\n3.3 V passive piezo",
     GREEN, 13)
card(s, 8.86, 1.72, 3.86, 2.08, "TIME CONTROL",
     "Minutes: 0–99\nSeconds: 0–59\nFischer increment: 0–60 s\nDefault: 5:00 + 0 s\nLow-time threshold: 30 s",
     GOLD, 13)
card(s, 0.62, 4.05, 3.86, 1.95, "SENSOR/TIME",
     "Optional DS3231 RTC\nI2C SDA: GPIO 23\nI2C SCL: GPIO 4\nSerial timestamps 115200 baud",
     CYAN, 13)
card(s, 4.74, 4.05, 3.86, 1.95, "NETWORK",
     "Wi-Fi STA, 2.4 GHz only\nHTTP port 80\nWebSocket port 81\nAdafruit HTTPS every 3 s\nGitHub Pages at 1 s poll",
     BLUE, 13)
card(s, 8.86, 4.05, 3.86, 1.95, "POWER/RECOVERY",
     "Light sleep: 2 idle minutes\nDeep sleep: 5 idle minutes\nWake GPIO: 27\nTask watchdog: 5 s\nCPU: 80 MHz",
     RED, 13)

# 5 Architecture
s = slide(prs, "Hardware Architecture", "Layered embedded-system block diagram",
          "Protocol boundaries prevent GPIO, display, persistence, networking, and recovery logic from becoming tightly coupled.", 5)
box(s, 0.62, 1.72, 2.35, 3.98, PANEL2, BLUE)
text(s, "INPUTS", 0.82, 1.96, 1.95, 0.28, 13, BLUE, True)
bullets(s, ["White / Black move", "Start / Pause / Reset", "Encoder A / B / switch",
            "GPIO pull-ups", "35 ms debounce"], 0.78, 2.42, 1.98, 2.60, 12)
box(s, 3.44, 1.72, 3.05, 3.98, PANEL2, GREEN)
text(s, "ESP32 CONTROL", 3.75, 1.96, 2.42, 0.28, 13, GREEN, True)
text(s, "Arduino loop task", 3.76, 2.48, 2.38, 0.30, 12, WHITE, True)
text(s, "FreeRTOS telemetry task", 3.76, 3.04, 2.38, 0.30, 12, WHITE, True)
text(s, "Game-state machine", 3.76, 3.60, 2.38, 0.30, 12, WHITE, True)
text(s, "NVS snapshot/settings", 3.76, 4.16, 2.38, 0.30, 12, WHITE, True)
text(s, "Task watchdog", 3.76, 4.72, 2.38, 0.30, 12, WHITE, True)
arrow(s, 2.98, 3.68, 3.42, 3.68, GREEN)
box(s, 7.02, 1.72, 2.40, 1.84, PANEL2, GOLD)
text(s, "TM1637", 7.32, 1.98, 1.80, 0.28, 13, GOLD, True)
text(s, "GPIO 18/19\nGPIO 25/26", 7.32, 2.52, 1.80, 0.50, 12, MUTED)
box(s, 7.02, 3.78, 2.40, 1.92, PANEL2, CYAN)
text(s, "DS3231 I2C", 7.32, 4.04, 1.80, 0.28, 13, CYAN, True)
text(s, "SDA 23 · SCL 4\nVCC/GND + pull-ups", 7.32, 4.58, 1.80, 0.62, 12, MUTED)
arrow(s, 6.50, 2.66, 7.00, 2.66, GOLD)
arrow(s, 6.50, 4.72, 7.00, 4.72, CYAN)
box(s, 9.96, 1.72, 2.76, 1.84, PANEL2, BLUE)
text(s, "LOCAL NETWORK", 10.28, 1.98, 2.15, 0.28, 13, BLUE, True)
text(s, "HTTP port 80\nWebSocket port 81", 10.28, 2.52, 2.15, 0.50, 12, MUTED)
box(s, 9.96, 3.78, 2.76, 1.92, PANEL2, GREEN)
text(s, "PUBLIC CLOUD", 10.28, 4.04, 2.15, 0.28, 13, GREEN, True)
text(s, "HTTPS · TLS\nAdafruit IO\nGitHub Pages", 10.28, 4.58, 2.15, 0.66, 12, MUTED)
arrow(s, 6.50, 3.05, 9.94, 2.66, BLUE)
arrow(s, 6.50, 4.18, 9.94, 4.72, GREEN)
text(s, "CLOCK-SENSITIVE PATH", 0.70, 6.16, 3.2, 0.25, 10, GREEN, True)
text(s, "GPIO → debounce → state → elapsed-time update → display → LEDs",
     0.70, 6.50, 11.7, 0.30, 13, WHITE, True)

# 6 Protocol integration
s = slide(prs, "Interface Engineering", "Communication protocols and pin roles",
          "Digital communication is selected by data type, throughput, timing, and peripheral requirements.", 6)
card(s, 0.62, 1.72, 3.86, 2.05, "TM1637",
     "Dedicated two-wire serial display protocol\nCLK/DIO, separate White/Black buses\nGPIO 18/19 and 25/26\nNot standard I2C",
     GOLD, 13)
card(s, 4.74, 1.72, 3.86, 2.05, "I2C / DS3231",
     "SDA GPIO 23 · SCL GPIO 4\nRTC clock and calendar register access\nWire timeout 1000 ms\nGraceful unavailable-RTC path",
     CYAN, 13)
card(s, 8.86, 1.72, 3.86, 2.05, "GPIO",
     "Pull-up buttons and encoder\nDebounced active-low controls\nLED outputs\nPiezo output GPIO 15\nWake GPIO 27",
     GREEN, 13)
card(s, 0.62, 4.10, 3.86, 1.88, "Wi-Fi STA",
     "Station mode and modem sleep\nOptional background connection\nClock remains independent\nESP32 is 2.4 GHz only",
     BLUE, 13)
card(s, 4.74, 4.10, 3.86, 1.88, "HTTP / WebSocket",
     "HTTP port 80 serves dashboard\nWebSocket port 81 broadcasts JSON\nRead-only browser connection\nlwIP initialized before server binding",
     GOLD, 13)
card(s, 8.86, 4.10, 3.86, 1.88, "HTTPS / TLS",
     "Adafruit API POST\nX-AIO-Key in header\nDigiCert root verification\nNo insecure() fallback",
     RED, 13)

# 7 Timing
s = slide(prs, "Real-Time Behavior", "Elapsed-time control and state transitions",
          "The clock is not based on one-second delays; state and wall-time are calculated in the main loop.", 7)
box(s, 0.62, 1.72, 5.85, 4.55, PANEL2, GREEN)
text(s, "NON-BLOCKING CLOCK UPDATE", 0.94, 1.98, 5.20, 0.28, 13, GREEN, True)
text(s, "elapsed = nowMs - lastClockUpdateMs",
     0.94, 2.50, 5.20, 0.34, 18, WHITE, True)
text(s, "activeTime -= elapsed\nlastClockUpdateMs = nowMs",
     0.94, 3.06, 5.20, 0.80, 18, CYAN, True)
text(s, "At ≤0: clamp to zero · set GAME_OVER\nbuild result · refresh both displays\nsound timeout alert · publish status",
     0.94, 4.20, 5.20, 1.20, 14, MUTED)
box(s, 6.78, 1.72, 5.94, 4.55, PANEL2, BLUE)
text(s, "MOVE EVENT", 7.10, 1.98, 5.30, 0.28, 13, BLUE, True)
steps = [
    "Measure turn thinking time",
    "Increment move count",
    "Update max/min/average statistics",
    "Apply active player's Fischer increment",
    "Switch active player",
    "Reset turn timer",
]
bullets(s, steps, 7.10, 2.48, 5.10, 2.70, 14, MUTED)
text(s, "Pause-safe: accumulated turn time is included",
     7.10, 5.46, 5.10, 0.38, 12, GREEN, True)

# 8 State machine
s = slide(prs, "Game-State Machine", "Mode-safe transitions prevent invalid clock action",
          "The firmware separates setup, idle, running, paused, and terminal states.", 8)
states = [
    ("SETUP", 0.78, 2.08, BLUE),
    ("READY", 3.18, 2.08, GOLD),
    ("RUNNING", 5.58, 2.08, GREEN),
    ("PAUSED", 8.00, 2.08, CYAN),
    ("GAME OVER", 10.58, 2.08, RED),
]
for label, x, y, color in states:
    box(s, x, y, 1.92, 0.72, PANEL, color)
    text(s, label, x + 0.08, y + 0.18, 1.76, 0.30, 11, color, True,
         PP_ALIGN.CENTER, MSO_ANCHOR.MIDDLE)
arrow(s, 2.72, 2.44, 3.16, 2.44, BLUE)
arrow(s, 5.12, 2.44, 5.56, 2.44, GOLD)
arrow(s, 7.52, 2.44, 7.98, 2.44, GREEN)
arrow(s, 9.94, 2.44, 10.56, 2.44, CYAN)
text(s, "Encoder commit", 2.20, 2.96, 1.60, 0.24, 9, MUTED, True, PP_ALIGN.CENTER)
text(s, "Start", 5.13, 2.96, 0.90, 0.24, 9, MUTED, True, PP_ALIGN.CENTER)
text(s, "Pause", 7.52, 2.96, 0.90, 0.24, 9, MUTED, True, PP_ALIGN.CENTER)
text(s, "Time = 0", 9.66, 2.96, 1.10, 0.24, 9, MUTED, True, PP_ALIGN.CENTER)
card(s, 0.62, 4.02, 3.86, 1.76, "RUNNING GUARDS",
     "Only active player's move button can switch turns. Starting is refused when either side is zero.",
     GREEN, 13)
card(s, 4.74, 4.02, 3.86, 1.76, "PAUSED GUARDS",
     "Turn countdown is frozen. Resume restores elapsed timing and preserves pause-aware thinking time.",
     CYAN, 13)
card(s, 8.86, 4.02, 3.86, 1.76, "TERMINAL GUARDS",
     "Timeout stops normal clock updates and requires Reset for a READY configuration.",
     RED, 13)

# 9 RTOS
s = slide(prs, "FreeRTOS & Concurrency", "The network cannot delay the clock",
          "An independent publisher task isolates blocking TLS/HTTP work from the real-time control loop.", 9)
box(s, 0.62, 1.72, 5.72, 4.56, PANEL2, GREEN)
text(s, "MAIN LOOP TASK", 0.94, 1.98, 5.08, 0.28, 13, GREEN, True)
bullets(s, [
    "Feeds the Arduino loop watchdog",
    "Services HTTP and WebSocket clients",
    "Debounces GPIO inputs",
    "Updates countdown and displays",
    "Updates LEDs and timeout state",
    "Queues only at 3-second telemetry intervals",
], 0.94, 2.48, 5.05, 2.82, 13)
chip(s, "CLOCK-PATH PRIORITY", 0.94, 5.72, 2.26, GREEN)
box(s, 6.78, 1.72, 5.94, 4.56, PANEL2, BLUE)
text(s, "TELEMETRY TASK · LOW PRIORITY", 7.10, 1.98, 5.30, 0.28, 13, BLUE, True)
text(s, "xQueueCreate(1, TelemetrySnapshot)",
     7.10, 2.54, 5.20, 0.34, 15, WHITE, True)
text(s, "xQueueOverwrite() keeps latest snapshot\nnot an accumulating history",
     7.10, 3.08, 5.20, 0.70, 14, MUTED)
text(s, "Wi-Fi check → NTP check → TLS connect\nHTTP POST → status-code handling",
     7.10, 4.14, 5.20, 0.84, 14, CYAN)
text(s, "Network result: success or explicit failure",
     7.10, 5.38, 5.20, 0.36, 12, GREEN, True)
text(s, "Isolation principle: one slow HTTPS request cannot delay a move or second update.",
     0.82, 6.50, 11.7, 0.30, 12, WHITE, True)

# 10 NVS
s = slide(prs, "Non-Volatile Storage", "NVS settings and deep-sleep snapshots",
          "ESP32 Preferences stores typed values and a validated binary structure in the chessclk namespace.", 10)
card(s, 0.62, 1.72, 3.86, 2.10, "PERSISTENT SETTINGS",
     "wmin · wsec · bmin · bsec\nwinc · binc\nWritten after encoder configuration\nRestored on boot and power-on",
     BLUE, 13)
card(s, 4.74, 1.72, 3.86, 2.10, "SLEEP SNAPSHOT",
     "state · active side\nremaining White/Black time\npause accumulator\ngame/RTC logging state\nmove statistics",
     GREEN, 13)
card(s, 8.86, 1.72, 3.86, 2.10, "VALIDATION",
     "Magic: 0x434C4B31\nVersion byte\nExact structure size\nREADY/PAUSED state only\nTime values must be ≥0",
     GOLD, 13)
box(s, 0.62, 4.16, 12.10, 1.76, PANEL2, LINE)
text(s, "BOOT RESTORE FLOW", 0.86, 4.43, 2.10, 0.25, 11, GREEN, True)
text(s, "load settings  →  detect snapshot marker  →  verify magic/version/size  →  restore or initialize  →  clear marker",
     0.86, 4.94, 11.46, 0.40, 16, WHITE, True)
text(s, "Benefits: pauses survive deep sleep while preventing stale/running-state restoration.",
     0.86, 5.48, 11.46, 0.24, 11, MUTED)

# 11 Networking
s = slide(prs, "Networking & Data Plane", "Local control status and public monitoring",
          "The firmware publishes snapshots at 3-second intervals and the public dashboard polls once per second.", 11)
nodes = [
    ("ESP32", 0.62, 2.40, 2.10, GREEN),
    ("JSON SNAPSHOT", 3.10, 2.40, 2.34, BLUE),
    ("ADAFRUIT IO", 6.04, 2.40, 2.26, GOLD),
    ("GITHUB PAGES", 9.14, 2.40, 2.50, CYAN),
]
for label, x, y, w, color in nodes:
    box(s, x, y, w, 1.16, PANEL2, color)
    text(s, label, x + 0.10, y + 0.40, w - 0.20, 0.30, 13, color, True,
         PP_ALIGN.CENTER, MSO_ANCHOR.MIDDLE)
arrow(s, 2.74, 2.98, 3.08, 2.98, GREEN)
arrow(s, 5.46, 2.98, 6.02, 2.98, BLUE)
arrow(s, 8.32, 2.98, 9.12, 2.98, GOLD)
card(s, 0.62, 4.20, 3.86, 1.72, "SNAPSHOT",
     "Clocks · state · result · active side · RTC status · configuration · move/think analytics",
     GREEN, 13)
card(s, 4.74, 4.20, 3.86, 1.72, "TLS / SECRETS",
     "Root CA verification · private AIO key in ignored secrets.h · public feed key/username in Pages config",
     GOLD, 13)
card(s, 8.86, 4.20, 3.86, 1.72, "PUBLIC UI",
     "Fetch last data record · JSON parse · 250 ms render loop · 1 s polling · stale/delayed/offline status",
     CYAN, 13)
text(s, "Browser security: public feed visibility is required. There is no key in public JavaScript.",
     0.82, 6.34, 11.50, 0.30, 11, RED, True)

# 12 Power
s = slide(prs, "Power Management", "Sleep policy aligned with game state",
          "Running and setup states remain awake; only idle READY/PAUSED states enter sleep.", 12)
levels = [
    ("80 MHz", "Normal/idle CPU", GREEN),
    ("MODEM SLEEP", "Wi-Fi radio only as needed", BLUE),
    ("LIGHT SLEEP", "No Wi-Fi · 2 min idle", GOLD),
    ("DEEP SLEEP", "State in NVS · 5 min idle", RED),
]
for i, (title, body, color) in enumerate(levels):
    x = 0.62 + i * 3.06
    box(s, x, 1.72, 2.58, 1.42, PANEL, color)
    text(s, title, x + 0.10, 2.02, 2.38, 0.30, 13, color, True,
         PP_ALIGN.CENTER)
    text(s, body, x + 0.12, 2.48, 2.34, 0.36, 11, WHITE,
         False, PP_ALIGN.CENTER)
    if i < 3:
        text(s, "↓", x + 2.64, 2.23, 0.34, 0.28, 16, MUTED, True,
             PP_ALIGN.CENTER)
card(s, 0.62, 3.72, 3.86, 1.82, "LIGHT SLEEP",
     "Disconnect Wi-Fi; enable GPIO wake on Start (GPIO 27); sync wake button state; then re-enable Wi-Fi and watchdog.",
     GOLD, 13)
card(s, 4.74, 3.72, 3.86, 1.82, "DEEP SLEEP",
     "Save settings and snapshot to NVS; blank displays; disable Wi-Fi; enable ext0 wake on GPIO 27.",
     RED, 13)
card(s, 8.86, 3.72, 3.86, 1.82, "WAKE SAFETY",
     "The wake press is consumed after synchronization. A separate Start press is needed to begin or resume.",
     GREEN, 13)
text(s, "Limitation: display supplies are not physically switched; a complete low-power product needs display/enclosure power switching.",
     0.82, 6.35, 11.50, 0.36, 11, GOLD, True)

# 13 Watchdog
s = slide(prs, "Fault Recovery", "ESP32 task watchdog and reset handling",
          "The watchdog detects software lockups in the main loop and forces a reset for a predictable recovery.", 13)
box(s, 0.62, 1.72, 12.10, 4.72, PANEL2, RED)
text(s, "WDT FEEDBACK LOOP", 0.94, 1.98, 3.00, 0.28, 13, RED, True)
stages = [
    ("NORMAL LOOP", "main loop executes", GREEN),
    ("FEED", "WDT reset fed automatically", BLUE),
    ("STALL", "loop blocked >5 s", GOLD),
    ("PANIC RESET", "watchdog timeout", RED),
    ("BOOT", "load state/settings", CYAN),
]
for i, (title, body, color) in enumerate(stages):
    x = 0.94 + i * 2.30
    box(s, x, 2.72, 1.86, 1.22, PANEL, color)
    text(s, title, x + 0.08, 2.98, 1.70, 0.25, 10, color, True,
         PP_ALIGN.CENTER)
    text(s, body, x + 0.10, 3.38, 1.66, 0.34, 9, WHITE,
         False, PP_ALIGN.CENTER)
    if i < 4:
        text(s, "→", x + 1.94, 3.14, 0.30, 0.28, 16, MUTED, True,
             PP_ALIGN.CENTER)
text(s, "enableLoopWDT() subscribes the Arduino loop task.",
     0.94, 4.42, 5.25, 0.30, 14, WHITE, True)
text(s, "disableLoopWDT() surrounds intentional light sleep.",
     0.94, 4.86, 5.25, 0.30, 14, WHITE, True)
text(s, "Re-enable immediately after wake.",
     0.94, 5.30, 5.25, 0.30, 14, GREEN, True)
text(s, "INTERRUPT WATCHDOG + CORE TASK WATCHDOG",
     7.14, 4.42, 5.10, 0.28, 11, GOLD, True)
text(s, "ESP-IDF also includes interrupt/task watchdog infrastructure; project firmware explicitly monitors and re-enables the main task.",
     7.14, 4.86, 5.10, 0.84, 13, MUTED)

# 14 Working
s = slide(prs, "Working Sequence", "Runtime from configuration to timeout",
          "A trace of the main sequence demonstrates how software states become physical actions.", 14)
steps = [
    ("1", "SETUP", "Restore NVS, initialize GPIO/I2C/Wi-Fi/servers, enable watchdog."),
    ("2", "CONFIGURE", "Encoder selects White/Black time fields and increments."),
    ("3", "READY", "Start begins countdown; active player is identified."),
    ("4", "RUNNING", "Move button records analytics, applies increment, switches active side."),
    ("5", "PAUSE", "Pause freezes turn; Start resumes elapsed-time logic."),
    ("6", "TIMEOUT", "Clamp time, set result, refresh displays, beep once, mark GAME OVER."),
]
y = 1.72
for i, (num, title, body) in enumerate(steps):
    color = [BLUE, GOLD, CYAN, GREEN, BLUE, RED][i]
    box(s, 0.62, y, 12.10, 0.70, PANEL, color)
    box(s, 0.82, y + 0.13, 0.44, 0.44, color, color)
    text(s, num, 0.84, y + 0.22, 0.40, 0.16, 10, BG, True,
         PP_ALIGN.CENTER, MSO_ANCHOR.MIDDLE)
    text(s, title, 1.48, y + 0.15, 1.72, 0.28, 13, color, True)
    text(s, body, 3.20, y + 0.14, 9.08, 0.38, 12, WHITE)
    y += 0.82

# 15 Validation
s = slide(prs, "Verification & Testing", "Evidence and test coverage",
          "Automated build checks validate compilation and syntax; hardware behavior requires device-level test evidence.", 15)
card(s, 0.62, 1.72, 3.86, 1.82, "BUILD",
     "ESP32 Dev Module / Arduino core 3.3.11\nSketch compiled successfully\n86% flash · 15% RAM at last watchdog build",
     GREEN, 13)
card(s, 4.74, 1.72, 3.86, 1.82, "SOFTWARE",
     "Dashboard JavaScript syntax checked\nPPTX archive and slide bounds validated\nTLS root certificate verified against Adafruit IO",
     BLUE, 13)
card(s, 8.86, 1.72, 3.86, 1.82, "RUNTIME",
     "Serial and browser status paths tested\nAdafruit publishing/configuration must be tested on a live public feed",
     GOLD, 13)
text(s, "HARDWARE TEST MATRIX", 0.72, 4.06, 3.50, 0.25, 11, CYAN, True)
tests = [
    "Countdown accuracy under 80 MHz operation",
    "Move switching and increment correctness",
    "Timeout display = 00:00 + one beep",
    "Wake button does not start/resume",
    "NVS restore after deep-sleep reset",
    "Watchdog reset under forced stall",
]
bullets(s, tests, 0.72, 4.48, 5.74, 1.78, 12, MUTED)
text(s, "LIMITATION OF CURRENT EVIDENCE", 6.80, 4.06, 3.80, 0.25, 11, RED, True)
text(s, "The firmware is compiled, but the presentation should not claim physical test results until individual buttons, sleep, buzzer, RTC, Wi-Fi, and watchdog are measured on the assembled board.",
     6.80, 4.48, 5.65, 1.35, 13, WHITE)

# 16 Scope
s = slide(prs, "Project Scope", "Implemented features and technical future work",
          "The current project is an embedded chess-clock platform; board sensing and long-term logging are future subsystems.", 16)
card(s, 0.62, 1.72, 5.78, 3.62, "IMPLEMENTED",
     "• Independent countdown state machine\n"
     "• Fischer increments and setup menu\n"
     "• TM1637 I/O, GPIO buttons, encoder, LEDs, buzzer\n"
     "• DS3231 I2C timestamps\n"
     "• NVS settings and deep-sleep snapshot\n"
     "• Local HTTP/WebSocket dashboard\n"
     "• FreeRTOS HTTPS telemetry task\n"
     "• Light/deep sleep and task watchdog",
     GREEN, 14)
card(s, 6.94, 1.72, 5.78, 3.62, "FUTURE / REPORT ITEMS",
     "• Bronstein delay\n"
     "• MicroSD/SPI game logging\n"
     "• Battery ADC and percentage estimation\n"
     "• MCP23017 I/O expansion\n"
     "• Reed/Hall sensor board detection\n"
     "• Display supply switching\n"
     "• External redundant watchdog",
     GOLD, 14)
text(s, "RECOMMENDED NEXT INTEGRATION",
     0.82, 5.82, 3.40, 0.25, 10, BLUE, True)
text(s, "Protect supply/strap pins → verify live hardware → characterize timing → add ADC battery monitoring → add SPI logging",
     0.82, 6.20, 11.50, 0.44, 13, WHITE, True)

prs.save(OUT)
print(f"Created {OUT}")
