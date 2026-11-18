# Football Shot Timer

A player dribbles past the sensor → a countdown starts → score before it hits zero.

- **On the box:** a 1-digit display shows the seconds, a knob sets them (1–9 s), a buzzer beeps.
- **On the phone (optional):** live countdown, settings and a loud horn.

---

## Parts

| Part | Where |
|---|---|
| ESP32-C3 Super Mini | Otronic |
| E18-D80NK infrared sensor | Otronic |
| Active buzzer module | Otronic |
| 7-segment display, 1 digit, common anode, 0.56" | Otronic |
| Rotary encoder HW-040 | Otronic |
| **7× 220 Ω resistor** (display) | Otronic "10x weerstand 220 Ohm" |
| 3× 10 kΩ resistor (sensor divider) | Otronic "10x weerstand 10K Ohm" |
| Breadboard + jumper wires | Otronic |
| USB power bank + USB-C cable | any store |

---

## Wiring

> ⚠️ Everything that connects to an ESP32 pin must stay at **3.3 V or less**.
> Only the sensor gets 5 V, and its signal goes through the divider.

### Power

| From | To |
|---|---|
| ESP32 **5V** | sensor brown wire |
| ESP32 **3V3** | display common pins (both), encoder **+**, buzzer **VCC** |
| ESP32 **GND** | sensor blue wire, encoder **GND**, buzzer **GND**, bottom of the divider |

### Sensor (E18-D80NK) — via the voltage divider

```
sensor black wire ──[10k]──┬── GPIO 3
                           │
                         [10k]
                           │
                         [10k]
                           │
                          GND
```

### Buzzer

| Buzzer pin | ESP32 |
|---|---|
| S / I/O / signal | GPIO 4 |
| VCC (if it has one) | 3V3 |
| GND / – | GND |

### Rotary encoder (HW-040)

| Encoder pin | ESP32 |
|---|---|
| CLK | GPIO 10 |
| DT | GPIO 2 |
| SW | GPIO 9 |
| + | **3V3** (not 5V!) |
| GND | GND |

GPIO 9 is also the board's BOOT button. That's fine, just **don't hold the knob pressed while you plug in the power**, or the board starts in programming mode.

### 7-segment display (common anode)

Each segment gets its own **220 Ω resistor** between the display pin and the ESP32 pin.

```
     a
   f   b          display pins (seen from the front, dot bottom right)
     g
   e   c          top row:     10  9  8  7  6
     d                          g  f COM a  b
                  bottom row:   1  2  3  4  5
                                e  d COM c  dp
```

| Segment | Display pin | ESP32 (via 220 Ω) |
|---|---|---|
| a | 7 | GPIO 0 |
| b | 6 | GPIO 1 |
| c | 4 | GPIO 5 |
| d | 2 | GPIO 6 |
| e | 1 | GPIO 7 |
| f | 9 | GPIO 20 |
| g | 10 | GPIO 21 |
| COM (pin 3 **and** 8) | | 3V3 (no resistor) |
| dp | 5 | not connected |

**Not sure of the display's pinout?** Connect COM to 3V3 and touch each other pin to GND through a 220 Ω resistor. Note which segment lights up.

When the board starts, it lights **a, b, c, d, e, f, g one by one**, then all of them. If the order is wrong, swap the wires (or the numbers in `PIN_SEG` in the code).

---

## Uploading the code

1. Install the **Arduino IDE** (arduino.cc).
2. *Settings → Additional boards manager URLs:*
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. *Tools → Board → Boards Manager* → install **esp32 by Espressif**.
4. Open `football_timer/football_timer.ino` (keep `page.h` in the same folder).
5. *Tools*:
   - Board: **ESP32C3 Dev Module**
   - USB CDC On Boot: **Enabled**
   - Port: the one that appears when you plug the board in
6. Click **Upload**.

**Upload doesn't start?** Unplug the board, hold the **BOOT** button, plug it in, release BOOT, then upload again.

No extra libraries are needed.

### Settings at the top of the code

| Setting | When to change it |
|---|---|
| `BUZZER_ACTIVE_HIGH` | `true` for the Otronic buzzer. If it beeps all the time and goes quiet when it should beep, set `false`. |
| `SENSOR_USE_INTERNAL_PULLUP` | Leave `false` when you use the divider. |
| `ENCODER_REVERSE` | Turning right makes the number go down? Set `true`. |
| `AP_SSID` / `AP_PASS` | The Wi-Fi name and password. |

---

## Using it

### The box

| Display | Meaning |
|---|---|
| `5` (steady) | **Ready.** Dribble past the sensor. |
| `-` | Waiting: something is in front of the sensor. Clear it for 1 second. |
| `4, 3, 2, 1` | Counting down. |
| `0` blinking | Time's up. |
| `P` | Paused. The sensor does nothing. |
| digit blinking fast | You're turning the knob to set the seconds. |

| Knob | Does |
|---|---|
| Turn | Set the seconds (1–9). Saved 2 seconds after you stop turning. |
| Press | Pause ↔ arm |
| Hold 1 s | Start the timer by hand (handy for testing) |

After each attempt, the box re-arms by itself after 2.5 seconds (you can turn this off on the phone).

**Beeps:** short beep = start, ticks on the last 3 seconds, long beep = time up. A tiny beep means the box is ready.

### The phone

1. Connect to the Wi-Fi **FootballTimer** (password `goal12345`).
   Your phone may warn "no internet". Choose to **stay connected**.
2. Open **http://192.168.4.1** in the browser.
3. Tap **Start** (this turns on the phone sound; on iPhone the ringer switch must be on).
4. Change the seconds or options, or tap **Start by hand** to test.

Several phones can be connected at the same time.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Display shows `-` all the time | The sensor always "sees" something: aim it away from the ground and turn its small screw (sensitivity) back. In the sun, put a short tube over it. On the phone, the pill at the top says *Clear* / *Blocked*. |
| Timer starts by itself | Same as above, or a loose wire on GPIO 3. |
| Segments light up wrong | Check the display pinout (see above). |
| Phone loses the Wi-Fi | Stay closer (the SuperMini has a small antenna), or check that the phone didn't switch back to your home Wi-Fi. |
| Power bank switches off by itself | Some power banks do this at low current. Try another one. |
