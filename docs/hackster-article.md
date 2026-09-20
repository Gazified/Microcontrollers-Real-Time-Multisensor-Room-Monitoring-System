# Building a Concurrent Real-Time Multisensor Room Monitor on ESP32 with Native FreeRTOS

**Author**: BCA152 Embedded Systems Student  
**Institution**: Mindanao State University - Iligan Institute of Technology (MSU-IIT), College of Computer Studies  
**Academic Collaborator / Reviewer**: Asst. Prof. Paul Rodolf P. Castor, M.Sc. (`paulrodolf.castor@g.msuiit.edu.ph`)  
**Target Hardware / Platform**: ESP32 DevKit-C v4, ESP-IDF Framework, FreeRTOS, Wokwi  

---

## 1. Project Overview
Most microcontroller projects start with a simple super-loop: read a sensor, delay for two seconds, print the output, update an OLED, and repeat. But as soon as you need to handle real-world physical events—like detecting instantaneous motion, listening to mechanical rotary encoder clicks, or evaluating critical temperature alarms—that single loop falls apart. Delays freeze the user interface, reading slow one-wire protocols skips encoder clicks, and code quickly turns into unmaintainable spaghetti.

In this project, I built a complete, production-style Room Multisensor on the ESP32 using native **ESP-IDF** and **FreeRTOS**. Instead of one monolithic loop, the application is engineered as six independent, concurrent tasks communicating over FreeRTOS queues, mutexes, and event groups.

---

## 2. Motivation
This project was developed as part of **BCA152 Microcontrollers** at Mindanao State University - Iligan Institute of Technology. The mission was to design an embedded firmware architecture from scratch that proves real-time concurrency, deterministic scheduling, and hardware-independent testability without relying on the Arduino framework.

---

## 3. Key Features
- **6 Concurrent FreeRTOS Tasks**: Clear task decomposition with explicit priority scheduling.
- **Microsecond 1-Wire Protocol for DHT22**: Native ESP-IDF bit-banged reader for ambient temperature and relative humidity.
- **Relative Light Monitoring**: 12-bit ADC acquisition from an analog photoresistor (LDR), normalized to 0–100%.
- **OLED UI Page Cycling**: Full 128x64 graphical display showing one metric at a time, navigable via rotary encoder with bidirectional wraparound.
- **Thermal Safety Alarm**: Evaluates temperature against strict laboratory thresholds (< 18.0°C or > 30.0°C) and activates a piezo buzzer.
- **Automatic Power Saving State Machine**: Automatically puts the OLED to sleep after 15 seconds of inactivity, waking immediately upon PIR motion.
- **Hardware-Independent Decision Logic**: Pure C++ functions verified by 13 automated unit tests.

---

## 4. Hardware and Components
1. **ESP32 DevKit-C v4**: Main dual-core microcontroller.
2. **DHT22 (AM2302)**: Digital humidity and temperature sensor.
3. **Photoresistor (LDR) Module**: Ambient illumination analog sensor.
4. **PIR Sensor**: Pyroelectric passive infrared motion detector.
5. **KY-040 Rotary Encoder**: Quadrature navigation dial with push-button.
6. **SSD1306 OLED (128x64, I2C)**: Monochrome graphical display.
7. **Piezo Buzzer**: Audible alarm annunciator.

---

## 5. Circuit Wiring

| Component | Pin | ESP32 GPIO |
|---|---|---|
| DHT22 | DATA | GPIO 4 |
| LDR Sensor | AO | GPIO 34 (ADC1_CH6) |
| PIR Sensor | OUT | GPIO 13 |
| Rotary Encoder | CLK | GPIO 18 |
| Rotary Encoder | DT | GPIO 19 |
| Rotary Encoder | SW | GPIO 5 |
| SSD1306 OLED | SDA | GPIO 21 |
| SSD1306 OLED | SCL | GPIO 22 |
| Buzzer | (+) | GPIO 23 |

All sensors share a common 3.3V VCC and GND bus from the ESP32 DevKit board.

---

## 6. FreeRTOS Architecture & Task Design

```
+-------------------------------------------------------------+
|                     FreeRTOS Scheduler                      |
+-------------------------------------------------------------+
   | Priority 3: InputTask (10ms)     -> Decodes Encoder UI
   | Priority 3: MotionTask (100ms)   -> Monitors PIR Sensor
   | Priority 2: SensorTask (2000ms)  -> Reads DHT22 & LDR ADC
   | Priority 2: AlarmTask (Queue)    -> Evaluates Temp & Buzzer
   | Priority 2: StateTask (1000ms)   -> 15s Inactivity Timeout
   | Priority 1: DisplayTask (150ms)  -> Renders SSD1306 OLED
```

### Why Dedicated Fan-Out Queues?
FreeRTOS queues remove items when read (`xQueueReceive`). If both `DisplayTask` and `AlarmTask` tried to read from one queue, whoever ran first would steal the sensor packet! To solve this, `SensorTask` posts to two dedicated queues: `xSensorQueueDisplay` and `xSensorQueueAlarm`.

### Why `vTaskDelayUntil()`?
Unlike `vTaskDelay()`, which delays relative to when it was called, `vTaskDelayUntil()` tracks the absolute scheduled tick. This guarantees that `SensorTask` samples every exact 2000 ms without accumulating execution time drift.

---

## 7. Testing and Verification

### 13 Automated Unit Tests
All decision logic was written as pure C++ functions and verified with automated unit tests using Unity:
- **Temperature Alarm Logic (5 Tests)**: Verified `< 18°C`, `== 18°C`, `24.5°C`, `== 30°C`, and `> 30°C`.
- **Navigation Logic (4 Tests)**: Verified forward cycling, reverse cycling, forward wraparound (`Motion -> Temp`), and reverse wraparound (`Temp -> Motion`).
- **State Logic (4 Tests)**: Verified active state persistence, 15-second inactivity timeout, sleep persistence, and motion awakening.

All 13 tests pass with 0 failures!

---

## 8. Lessons Learned & Fault Experiments
1. **Never create uncontrolled busy loops**: When I commented out the delay in `SensorTask`, it starved the FreeRTOS IDLE task on Core 0, triggering the Task Watchdog reset within 5 seconds.
2. **Priorities reflect urgency, not popularity**: Elevating `DisplayTask` above `InputTask` caused rotary encoder turns to lag noticeably during I2C transfers. Giving `InputTask` Priority 3 and `DisplayTask` Priority 1 kept the UI completely responsive.
3. **Always protect shared streams**: Without `xSerialMutex`, concurrent `printf` statements from multiple tasks mangled the serial output.

---

## 9. GitHub Repository
The complete source code, Wokwi circuit simulation files, unit tests, and academic report are published on GitHub:
👉 `https://github.com/YOUR_USERNAME/bca152-freertos-multisensor`

---

## 10. Acknowledgments & Accreditation
Developed for **BCA152 Microcontrollers (Laboratory Activity No. 1)** under the instruction of **Asst. Prof. Paul Rodolf P. Castor, M.Sc.** (`paulrodolf.castor@g.msuiit.edu.ph`), Department of Computer Applications, College of Computer Studies, Mindanao State University - Iligan Institute of Technology.
