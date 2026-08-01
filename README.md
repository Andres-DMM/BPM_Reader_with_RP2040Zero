# 🫀 RP2040 Zero - 20s BPM Monitor

An embedded, non-blocking heart rate monitor powered by the **Waveshare RP2040 Zero**, a **MAX30102 Pulse Oximeter sensor**, and a **0.96" SSD1306 OLED display**.

Featuring a smart finger-detection workflow, real-time beat calculations, 20-second dynamic sample averaging, and an onboard NeoPixel breathing LED indicator.

---

## 🌟 Key Features

- **Dual I2C Bus Configuration:** Runs the OLED display on `Wire` (`I2C0`) and the pulse sensor on `Wire1` (`I2C1`) natively.
- **Auto Finger Trigger:** Starts a 20-second continuous sampling sequence automatically upon finger detection.
- **Live Filtering:** Rejects noise and unrealistic heart rate spikes outside the physiological window (45–170 BPM).
- **RGB Breathing Indicator:** Pulses the onboard WS2812 NeoPixel LED (GP16) using a smooth red fading pattern during active measurements.
- **10-Second Display Hold:** Summarizes the computed 20-second average on the OLED for 10 seconds post-measurement.
- **Smart Reset Cycle:** Requires complete finger removal before resetting to the idle screen to prevent false re-triggers.

---

## 🛠️ Hardware & Pinout

| Component | Interface / Pin | Default Bus | Address |
| :--- | :--- | :--- | :--- |
| **SSD1306 OLED (128x64)** | SDA: `GP4` / SCL: `GP5` | `Wire` (`I2C0`) | `0x3C` |
| **MAX30102 Sensor** | SDA: `GP2` / SCL: `GP3` | `Wire1` (`I2C1`) | `0x57` |
| **Status NeoPixel** | Data: `GP16` | Onboard RGB | — |

---

## 📦 Required Arduino Libraries

Ensure the following libraries are installed via the **Arduino Library Manager** (`Ctrl+Shift+I` / `Cmd+Shift+I`):

1. **SparkFun MAX3010x Pulse and Proximity Sensor Library** by *SparkFun*
2. **Adafruit SSD1306** by *Adafruit*
3. **Adafruit GFX Library** by *Adafruit*
4. **Adafruit NeoPixel** by *Adafruit*

---

## 🔄 Execution Workflow

- [ IDLE STATE ]
- └─► Waiting for finger placement (IR > 20,000 threshold)
### │
### ▼
### [ 20s MEASUREMENT ]
### ├─► NeoPixel LED red breathing animation
### ├─► Non-blocking warning on movement ("Keep Finger Still!")
### └─► Live BPM sampling & running sum calculation
### │
### ▼
### [ 10s RESULT HOLD ]
### └─► Displays calculated 20s Average BPM
### │
### ▼
### [ FINGER REMOVAL ]
### └─► Prompts user to remove finger to reset back to IDLE


---

## 💡 Sensor Usage Tips

- **Light Contact:** Rest your finger lightly on the sensor glass. Pressing too hard flattens blood vessels and disrupts the IR readings.
- **Shield Ambient Light:** Ensure your finger completely covers both the optical emitter and receiver. Excessive room lighting can alter sensor noise levels.

---

## 📄 License

Distributed under the **MIT License**. Feel free to modify and build upon it!
