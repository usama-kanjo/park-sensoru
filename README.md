# Park Sensoru — ESP32 Parking Sensor

ESP32-based parking assistant with ultrasonic distance sensing, WiFi web dashboard, and deep sleep power saving.

## Features

- **HC-SR04 Ultrasonic Distance Sensor** — measures 2–400 cm
- **Potentiometer Threshold** — adjust sensitivity (10–150 cm) in real-time
- **PWM Buzzer** — beeps faster as you get closer to an obstacle
- **Web Dashboard** — live distance display with color-coded status (green/yellow/red)
  - WebSocket real-time updates
  - REST API at `/api`
  - Responsive mobile-friendly UI
- **Deep Sleep** — automatically sleeps after 120 seconds of inactivity; wakes every 10 seconds to check for movement
- **On-board LED** — visual status indicator

## Branches

This repo has two branches with different WiFi modes:

| Branch | WiFi Mode | Description |
|--------|-----------|-------------|
| **`main`** | AP (Access Point) | ESP32 creates its own network — no router needed. SSID: `ParkSensoru`, password: `123456789`. Connect directly and open `http://192.168.4.1`. |
| **`router-mode`** | STA (Station) | ESP32 connects to your home WiFi via WiFiManager. On first boot, it starts a captive portal for configuration. |

**Switch between branches:**
```bash
git checkout main          # AP mode
git checkout router-mode   # Router mode
```

## Hardware

| Component | Pin |
|-----------|-----|
| HC-SR04 TRIG | GPIO 5 |
| HC-SR04 ECHO | GPIO 18 |
| Buzzer (PWM) | GPIO 19 |
| Potentiometer | GPIO 34 (ADC) |
| Built-in LED | GPIO 2 |

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- ESP32 dev board

### Build & Upload

```bash
pio run --target upload
```

### Monitor

```bash
pio device monitor
```

### Usage (`main` — AP mode)

1. Power the ESP32 — it starts an access point named **ParkSensoru** (password: `123456789`).
2. Connect your phone/computer to that WiFi network.
3. Open `http://192.168.4.1` in any browser.
4. The dashboard shows real-time distance with color-coded status.

### Usage (`router-mode` — STA mode)

1. Flash the `router-mode` branch.
2. On first boot, ESP32 starts a config portal named **ParkSensoru** (no password).
3. Connect to it, open `http://192.168.4.1`, and enter your home WiFi credentials.
4. ESP32 reboots and connects to your router.
5. Find its IP from the serial monitor and open it in a browser.

## API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web dashboard |
| `/api` | GET | JSON `{"d":<cm>,"t":<threshold>}` |
| `/ws` | WebSocket | Real-time updates `{"d":<cm>,"t":<threshold>}` |

## Power Saving

After 120 seconds of no significant distance change (>3 cm), the ESP32 enters deep sleep. It wakes every 10 seconds to take a quick measurement:

- **Movement detected** → stays awake
- **No movement** → goes back to sleep

## License

MIT
