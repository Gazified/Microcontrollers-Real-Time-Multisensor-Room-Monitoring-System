# Real-Time Multisensor Room Monitoring System

**Course**: BCA152 Microcontrollers — Laboratory Activity No. 1  
**Institution**: Mindanao State University - Iligan Institute of Technology (MSU-IIT), College of Computer Studies  
**Author**: BCA152 Student  
**Instructor / Collaborator**: Asst. Prof. Paul Rodolf P. Castor, M.Sc. (`paulrodolf.castor@g.msuiit.edu.ph`)  
**Target Platform**: Espressif ESP32 (ESP-IDF v5+ / Native FreeRTOS)  
**Simulation Environment**: Wokwi Embedded Simulator  

---

## Project Overview

In this laboratory activity, I designed and implemented a real-time room environmental monitor from scratch using an ESP32 microcontroller, native ESP-IDF, and FreeRTOS concurrency primitives. Instead of writing a traditional monolithic Arduino sketch (`setup()` + large `loop()` with delays), this firmware runs as a true real-time multitasking system split across six dedicated FreeRTOS tasks.

The system periodically captures environmental metrics (temperature and relative humidity from a DHT22, and ambient light intensity from an LDR sensor via the ESP32 ADC), evaluates safety alarm conditions, processes continuous user rotary encoder input, monitors room occupancy via a PIR motion detector, updates an SSD1306 128x64 OLED display over I2C, and manages automated power-saving states (ACTIVE vs. INACTIVE) with a 15-second inactivity timeout.

---

## Features

- **True Concurrency**: Firmware execution partitioned into 6 distinct, cooperatively scheduled FreeRTOS tasks with explicit priorities.
- **Hardware-Independent Decision Logic**: Pure functions for temperature alarm thresholds, display page navigation, and system state transitions, verified with 13 automated unit tests.
- **Inter-Task Communication (Fan-out IPC)**: Sensor readings are dispatched from `SensorTask` to independent consumer queues for `DisplayTask` and `AlarmTask` to prevent queue stealing.
- **Resource Synchronization**: Shared UART standard output protected by a FreeRTOS mutex (`xSerialMutex`) preventing interleaved serial logging.
- **System Event Coordination**: FreeRTOS Event Group (`xSystemEvents`) tracking ACTIVE state, motion pulses, and thermal alarm status.
- **Deterministic Periodic Timing**: `SensorTask` utilizes `vTaskDelayUntil()` to eliminate timing drift across sensor acquisition intervals.
- **Power State Machine**: Automatically transitions to low-power `INACTIVE` state (blanking OLED) after 15 seconds of silence, restoring instantly upon PIR motion trigger.
- **Rotary Encoder UI Navigation**: Bi-directional continuous page cycling (`Temperature -> Humidity -> Light -> Motion -> Temperature`) with wraparound in both directions.

---

## Learning Objectives

1. Configure and build a native ESP-IDF C++ project using PlatformIO and compile without Arduino dependencies.
2. Construct and verify an interactive multisensor hardware circuit in Wokwi.
3. Partition embedded firmware into modular subsystems with clear header/source separation.
4. Implement and justify task priorities, task states (Running, Ready, Blocked), and scheduler mechanics.
5. Apply FreeRTOS queues, mutexes, event groups, and `vTaskDelayUntil()` to resolve concrete embedded synchronization challenges.
6. Write automated unit tests for core decision logic without physical hardware dependencies.
7. Conduct deliberate FreeRTOS fault experiments (CPU starvation, priority inversion, race conditions) to understand real-time failure modes.

---

## System Architecture

The following diagram illustrates the hardware and data flow across the simulated peripherals:

```
                          +-------------------------+
                          |   Simulated Hardware    |
                          +------------+------------+
                                       |
       +-------------------------------+-------------------------------+
       |                               |                               |
       v                               v                               v
 [ DHT22 Sensor ]              [ LDR Light Sensor ]             [ PIR Motion ]
  Temp & Humidity               Analog Voltage (AO)              Digital Pulse
       |                               |                               |
 (GPIO 4 / 1-Wire)              (GPIO 34 / ADC1)                (GPIO 13 / IN)
       |                               |                               |
       +---------------+---------------+                               |
                       |                                               |
                       v                                               v
               +---------------+                               +---------------+
               |  SensorTask   |                               |  MotionTask   |
               +-------+-------+                               +-------+-------+
                       |                                               |
       +---------------+---------------+                               |
       | (Queue)                       | (Queue)                       |
       v                               v                               |
+--------------+               +---------------+                       |
| DisplayTask  |               |   AlarmTask   |                       |
+-------+------+               +-------+-------+                       |
        |                              |                               |
  (I2C 21/22)                      (GPIO 23)                           |
        v                              v                               |
 [ SSD1306 OLED ]               [ Buzzer Alarm ]                       |
  128x64 Graphics                Thermal Warning                       |
        ^                                                              |
        | Page Navigation                                              |
+-------+------+                                                       |
|  InputTask   |                                                       v
+--------------+                                               +---------------+
        ^                                                      |   StateTask   |
        |                                                      +---------------+
 (GPIO 18, 19, 5)                                                      |
 [ Rotary Enc. ]                                             ACTIVE / INACTIVE State
```

---

## FreeRTOS Architecture

### Task Execution & Priority Assignment

| Task Name | Priority | Core | Period / Trigger | IPC / Sync Primitive | Blocked Condition | Responsibility |
|---|---|---|---|---|---|---|
| **`InputTask`** | 3 | Core 1 | 10 ms polling | Direct shared state / UART mutex | `vTaskDelay(10ms)` | Detects quadrature edges on rotary encoder; handles CW/CCW page navigation |
| **`MotionTask`** | 3 | Core 1 | 100 ms polling | Event Group (`EVENT_MOTION`) | `vTaskDelay(100ms)` | Samples PIR sensor; sets/clears motion bits; awakens inactive system |
| **`SensorTask`** | 2 | Core 0 | Periodic 2000 ms | Fan-out Queues (`xSensorQueueDisplay`, `xSensorQueueAlarm`) | `vTaskDelayUntil()` | Reads DHT22 and LDR; packages `SensorData`; broadcasts to consumers |
| **`AlarmTask`** | 2 | Core 0 | Event-driven (Queue) | `xSensorQueueAlarm`, Event Group (`EVENT_ALARM`) | `xQueueReceive(portMAX_DELAY)` | Evaluates temperature threshold (`<18°C` or `>30°C`); triggers buzzer |
| **`StateTask`** | 2 | Core 0 | Periodic 1000 ms | Event Group (`EVENT_ACTIVE`, `EVENT_MOTION`) | `vTaskDelay(1000ms)` | Evaluates inactivity timeout (15s); transitions between ACTIVE & INACTIVE |
| **`DisplayTask`**| 1 | Core 1 | 150-300 ms refresh | `xSensorQueueDisplay`, Event Group (`EVENT_ACTIVE`) | `xQueueReceive(300ms)` | Owns SSD1306 OLED; renders active metric screen or sleep banner |

### Priority Justification

- **Priority 3 (Highest - User & Event Response)**: `InputTask` and `MotionTask` must react promptly to user mechanical interactions and physical motion pulses. If an encoder turn is delayed, ticks are skipped. Because their duty cycle per wake is minuscule (<100 microseconds), they immediately yield and do not starve lower tasks.
- **Priority 2 (Medium - Processing & Safety)**: `SensorTask`, `AlarmTask`, and `StateTask` handle environmental evaluation and system state tracking. Safety decisions (e.g. over-temperature alarm) require prompt execution once sensor readings arrive.
- **Priority 1 (Lowest - Visual Interface)**: `DisplayTask` handles rendering over I2C. Visual updates can tolerate a few tens of milliseconds of latency without human perception, making it safe to yield CPU cycles to real-time inputs.

---

## Hardware / Simulated Components

1. **ESP32 DevKit-C v4**: Dual-core Xtensa LX6 microcontroller running at 160/240 MHz.
2. **DHT22 (AM2302)**: Digital humidity and temperature sensor utilizing custom single-wire bus protocol.
3. **Photoresistor (LDR) Module**: Ambient light sensor providing analog voltage output.
4. **PIR Motion Sensor**: Passive infrared motion sensor generating active-high signal upon movement.
5. **Rotary Encoder (KY-040)**: Incremental mechanical encoder with quadrature phase outputs and integrated momentary push button.
6. **SSD1306 OLED Display**: 0.96-inch monochrome 128x64 display interfaced via I2C bus at address `0x3C`.
7. **Piezo Buzzer**: Audible indicator triggered when ambient temperature exceeds predefined safety limits.

---

## Pin Configuration

| Component | Signal Name | ESP32 GPIO | Mode / Function |
|---|---|---|---|
| **DHT22** | DATA / SDA | `GPIO 4` | Bidirectional Open-Drain (Microsecond protocol) |
| **Photoresistor (LDR)** | Analog Out (AO) | `GPIO 34` | ADC1 Channel 6 (12-bit, 0–4095) |
| **PIR Sensor** | OUT | `GPIO 13` | Digital Input (Internal Pulldown) |
| **Rotary Encoder** | CLK (Phase A) | `GPIO 18` | Digital Input (Internal Pullup) |
| **Rotary Encoder** | DT (Phase B) | `GPIO 19` | Digital Input (Internal Pullup) |
| **Rotary Encoder** | SW (Button) | `GPIO 5` | Digital Input (Internal Pullup) |
| **SSD1306 OLED** | SDA | `GPIO 21` | I2C Master Data |
| **SSD1306 OLED** | SCL | `GPIO 22` | I2C Master Clock |
| **Buzzer** | Signal (+) | `GPIO 23` | Digital Output |

---

## Inter-Task Communication

### Fan-out Queue Strategy

The laboratory handout notes that a standard FreeRTOS queue read consumes the message (`xQueueReceive` removes the item). Because both `DisplayTask` and `AlarmTask` need the latest `SensorData`, a single shared queue would create a race condition where whichever task awakens first "steals" the data from the other.

To solve this, I designed an explicit fan-out queue architecture:
- `xSensorQueueDisplay` (Capacity: 5 `SensorData` structs)
- `xSensorQueueAlarm` (Capacity: 5 `SensorData` structs)

`SensorTask` writes to both queues simultaneously using `xQueueOverwrite()`, guaranteeing that both consumers always receive fresh data independently.

### Mutex Synchronization

Multiple tasks (`SensorTask`, `InputTask`, `MotionTask`, `AlarmTask`, `StateTask`) produce diagnostic log messages sent over UART `stdout`. Without synchronization, output characters and lines interleave unpredictably. 

I implemented `safeSerialPrintf()` protected by `xSerialMutex`:
```cpp
void safeSerialPrintf(const char *format, ...) {
    if (xSemaphoreTake(xSerialMutex, portMAX_DELAY) == pdTRUE) {
        // format and write to UART
        fflush(stdout);
        xSemaphoreGive(xSerialMutex);
    }
}
```

### Event Group Signaling

`xSystemEvents` coordinates system-wide state flags:
- `BIT0 (EVENT_ACTIVE)`: Set when room is active; cleared when system enters 15-second inactivity sleep.
- `BIT1 (EVENT_MOTION)`: Set when PIR detects movement; cleared when movement ceases.
- `BIT2 (EVENT_ALARM)`: Set when temperature is outside the 18°C–30°C normal range.

---

## State Machine

```
              +--------------------------+
              |          ACTIVE          |
              |  - OLED Display Active   |
              |  - Sensor Updates Live   |
              |  - Rotary Nav Enabled    |
              |  - Alarm Monitoring On   |
              +-------------+------------+
                            |
                            | Inactivity Timeout (15s without motion)
                            v
              +--------------------------+
              |         INACTIVE         |
              |  - OLED Display Sleeping |
              |  - Low Refresh Overhead  |
              |  - PIR Monitoring Alive  |
              +-------------+------------+
                            |
                            | Motion Detected (PIR high pulse)
                            +---------------------------> ACTIVE
```

---

## Repository Structure

```
bca152-freertos-multisensor/
├── CMakeLists.txt             # ESP-IDF root build system descriptor
├── platformio.ini             # PlatformIO environments (esp32dev + native)
├── diagram.json               # Full Wokwi simulation circuit configuration
├── wokwi.toml                 # Wokwi firmware and ELF target paths
├── .gitignore                 # Excludes build caches (.pio), IDE files, binaries
├── README.md                  # Public technical documentation & portfolio
├── include/
│   ├── rtos_objects.h         # RTOS handles, synchronization primitives, event bits
│   ├── system_state.h         # SystemState enum and evaluateSystemState() prototype
│   ├── sensors.h              # SensorData struct, DHT22/LDR driver prototypes
│   ├── display.h              # DisplayMode enum, navigation functions, OLED driver
│   ├── input.h                # Rotary encoder interface
│   ├── alarm.h                # AlarmState enum, evaluateTemperature() prototype
│   └── motion.h               # PIR sensor interface
├── src/
│   ├── CMakeLists.txt         # ESP-IDF source component registration
│   ├── main.cpp               # app_main(): RTOS initialization and task creation
│   ├── rtos_objects.cpp       # Mutex, queue, and event group instantiations
│   ├── system_state.cpp       # StateTask and evaluateSystemState() logic
│   ├── sensors.cpp            # SensorTask with vTaskDelayUntil(), DHT22, LDR ADC
│   ├── display.cpp            # DisplayTask and SSD1306 I2C native rendering
│   ├── input.cpp              # InputTask and quadrature decoder logic
│   ├── alarm.cpp              # AlarmTask and evaluateTemperature() logic
│   └── motion.cpp             # MotionTask and PIR edge detection
├── test/
│   ├── unity.h                # Unity test framework definitions
│   ├── unity.c                # Unity test harness implementation
│   └── test_main.cpp          # 13 automated unit tests for decision logic
└── docs/
    └── laboratory-report.md    # Comprehensive academic laboratory report
```

---

## Getting Started

### Prerequisites

- [PlatformIO Core](https://platformio.org/) (`pio` CLI or VS Code extension)
- Git
- GCC / MinGW (for native unit testing)
- [Wokwi Simulator](https://wokwi.com/) (or VS Code Wokwi extension)

### Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/bca152-freertos-multisensor.git
cd bca152-freertos-multisensor
```

---

## Building the Project

Compile the ESP32 ESP-IDF firmware:

```bash
pio run -e esp32dev
```

This compiles all modular sources, initializes FreeRTOS components, and produces `.pio/build/esp32dev/firmware.bin`.

---

## Running the Wokwi Simulation

1. Open the repository folder in VS Code.
2. Ensure the Wokwi extension is installed and activated.
3. Open `diagram.json`.
4. Press `F1` and select **Wokwi: Start Simulator**, or click the Play icon.
5. Observe boot diagnostics in the Serial Monitor at 115200 baud.

---

## Unit Testing

Run the 13 automated unit tests directly:

```bash
g++ -Iinclude -Itest test/test_main.cpp test/unity.c src/alarm.cpp src/display.cpp src/system_state.cpp -o test_runner.exe
./test_runner.exe
```

### Test Results

```
=======================================================
        BCA152 FIRMWARE LOGIC AUTOMATED UNIT TESTS     
=======================================================
TEST(test_alarm_below_lower_threshold): PASS
TEST(test_alarm_exactly_lower_threshold): PASS
TEST(test_alarm_normal_value): PASS
TEST(test_alarm_exactly_upper_threshold): PASS
TEST(test_alarm_above_upper_threshold): PASS
TEST(test_navigation_forward_transition): PASS
TEST(test_navigation_reverse_transition): PASS
TEST(test_navigation_forward_wraparound): PASS
TEST(test_navigation_reverse_wraparound): PASS
TEST(test_state_active_no_timeout): PASS
TEST(test_state_active_timeout): PASS
TEST(test_state_inactive_no_motion): PASS
TEST(test_state_inactive_motion): PASS
-------------------------------------------------------
13 Tests 0 Failures 0 Ignored
RESULT: ALL UNIT TESTS PASSED (OK)
=======================================================
```

---

## Static Code Analysis

Static analysis was performed using PlatformIO Check (`pio check` / `cppcheck`):

```bash
pio check -e native
```

### Static Analysis Findings Table

| Finding | File / Line | Cause | Resolution |
|---|---|---|---|
| Style / unused parameter | `alarm.cpp:pvParameters` | FreeRTOS task function prototype requires `void *pvParameters` | Explicitly marked `(void)pvParameters;` |
| Style / unused parameter | `display.cpp:pvParameters` | FreeRTOS task function prototype requires `void *pvParameters` | Explicitly marked `(void)pvParameters;` |
| Style / unused parameter | `sensors.cpp:pvParameters` | FreeRTOS task function prototype requires `void *pvParameters` | Explicitly marked `(void)pvParameters;` |
| Type safety / conversion | `display.cpp:oledWriteData` | `const_cast` used on buffer parameter for I2C API | Safely cast pointer to match ESP-IDF driver prototype |
| Variable scope | `sensors.cpp:waitPinState` | Static helper functions exposed at translation unit level | Made `static` to restrict linkage to translation unit |

---

## Functional Verification

The firmware was verified against all requirements using the baseline verification matrix:

| ID | Stimulus | Expected Result | Actual Observed Behavior | Result |
|---|---|---|---|---|
| **FT-01** | Change DHT22 temperature slider to 28.5°C | Displayed temperature updates to 28.5°C | OLED page displays `Temp: 28.5 C` within 2 seconds | **PASS** |
| **FT-02** | Change DHT22 humidity slider to 72% | Displayed humidity updates to 72% | OLED page displays `Hum : 72.0 %` on humidity screen | **PASS** |
| **FT-03** | Slide LDR ambient light input | Light percentage changes from 0% to 100% | ADC raw value scales smoothly to percentage on OLED | **PASS** |
| **FT-04** | Rotate encoder clockwise | Next screen selected (`Temp -> Hum -> Light -> Motion -> Temp`) | Page changes in correct order; wraps from Motion to Temp | **PASS** |
| **FT-05** | Rotate encoder counterclockwise | Previous screen selected (`Temp -> Motion -> Light -> Hum -> Temp`) | Page traverses in reverse order; wraps from Temp to Motion | **PASS** |
| **FT-06** | Set temperature to 35°C (> 30°C limit) | Buzzer activates, warning logged | GPIO 23 turns HIGH, buzzer sounds, OLED shows `! TEMP ALARM !` | **PASS** |
| **FT-07** | Return temperature to 24°C (normal range) | Buzzer deactivates | GPIO 23 turns LOW, buzzer stops, status line returns to normal | **PASS** |
| **FT-08** | Trigger PIR motion sensor | System is marked ACTIVE | `EVENT_MOTION` and `EVENT_ACTIVE` bits set, status verified | **PASS** |
| **FT-09** | Wait 15 seconds without PIR motion | System enters INACTIVE state | OLED screen blanks to sleep banner, power-saving mode active | **PASS** |
| **FT-10** | Trigger PIR motion while INACTIVE | System instantly awakens to ACTIVE | OLED re-activates immediately, displaying current metrics | **PASS** |

---

## Engineering Decisions

1. **Separation of Business Logic from Hardware**: Rather than tightly coupling decision logic with hardware calls inside task loops, functions like `evaluateTemperature()`, `nextDisplayMode()`, and `evaluateSystemState()` are implemented as pure, deterministic C++ functions. This enabled fast automated unit testing on the host workstation.
2. **Periodic Execution using `vTaskDelayUntil()`**: Standard `vTaskDelay()` introduces cumulative timing drift because the delay interval begins *after* sensor reading completes. `vTaskDelayUntil()` enforces a fixed period referenced to the scheduled wake tick, ensuring exact 2000 ms sampling intervals regardless of 1-wire communication execution time.
3. **Dedicated Fan-Out Consumer Queues**: FreeRTOS queues operate as FIFO structures where reading an item removes it from the queue. To enable both `DisplayTask` and `AlarmTask` to consume fresh sensor packets without data starvation, dedicated queues were implemented.

---

## Limitations

- **Simulated Environmental Dynamics**: In Wokwi, sensor values are driven by interactive sliders and step toggles rather than real-world physical thermodynamic and photometric fluctuations.
- **LDR Light Calibration**: The LDR measurement is expressed as a normalized relative percentage (0–100%) derived from the 12-bit ADC reading. It does not represent absolute physical illuminance (lux) because calibration curves and lux conversion tables were not fitted against a laboratory luxmeter.

---

## Future Improvements

1. Implement non-volatile storage (NVS) to allow user-configurable temperature thresholds over serial command line.
2. Add an SSD1306 graphical line plot of historical temperature trends across the last 60 seconds.
3. Integrate MQTT telemetry over Wi-Fi for remote IoT dashboard monitoring.

---

## References and Acknowledgments

- Mindanao State University - Iligan Institute of Technology, College of Computer Studies, Department of Computer Applications: *BCA152 Microcontrollers, Laboratory Activity No. 1 — Real-Time Multisensor Room Monitoring System*, Prepared by Asst. Prof. Paul Rodolf P. Castor, M.Sc., September 2026.
- FreeRTOS Kernel Developer Guide: *Tasks, Queues, Semaphores, and Event Groups*, Real Time Engineers Ltd.
- Espressif Systems: *ESP-IDF Programming Guide (v5.x)*.
- Wokwi Documentation: *ESP32 Simulated Hardware and Peripherals Reference*.
