# Laboratory Activity Report No. 1
## Real-Time Multisensor Room Monitoring System

**Course**: BCA152 Microcontrollers  
**Institution**: Mindanao State University - Iligan Institute of Technology  
**College / Department**: College of Computer Studies / Department of Computer Applications  
**Student Name**: BCA152 Student  
**Instructor**: Asst. Prof. Paul Rodolf P. Castor, M.Sc.  
**Date of Submission**: September 2026  

---

### Executive Summary

This report documents the architectural design, implementation, and rigorous verification of a real-time environmental room monitoring system built on the ESP32 microcontroller using native ESP-IDF and FreeRTOS. The project transitions away from monolithic, super-loop embedded architectures to a concurrent multitasking system comprising six cooperating FreeRTOS tasks. The system captures ambient temperature and relative humidity via a DHT22 sensor, relative illumination via an LDR analog circuit, and room occupancy via a PIR motion detector. Data is output on an I2C SSD1306 OLED display and an audible buzzer alarm. Synchronization and inter-task communication are managed using dedicated consumer queues, a logging mutex, and a system event group. All business decision logic was verified using 13 automated unit tests, static code analysis was performed using PlatformIO Check, and three deliberate fault injection experiments were conducted to analyze real-time failure modes.

---

### 1. Problem and Requirements

#### 1.1 Problem Statement
Typical introductory microcontroller implementations rely on a single execution loop where sensor acquisition, data processing, user input handling, and display updates execute sequentially. In such systems, long-duration or blocking operations—such as DHT22 single-wire protocol handshakes or display bus transactions—induce unacceptable latency, miss user interactions, and cause erratic timing drift. 

The objective of this laboratory activity is to reconstruct and implement a robust, concurrent room monitoring system satisfying real-time constraints, deterministic scheduling, safe shared-resource access, and modular software boundaries.

#### 1.2 System Requirements
The system must satisfy ten key functional requirements:
- **FR-01 (Temperature Measurement)**: Periodically read temperature from DHT22.
- **FR-02 (Humidity Measurement)**: Periodically read relative humidity from DHT22.
- **FR-03 (Ambient-Light Measurement)**: Monitor relative illumination via photoresistor/LDR.
- **FR-04 (Motion Detection)**: Detect simulated human presence via a PIR motion detector.
- **FR-05 (OLED Display)**: Present individual measurement pages on an SSD1306 128x64 display.
- **FR-06 (Rotary Encoder Navigation)**: Allow bidirectional cycling through measurement pages with wraparound.
- **FR-07 (Temperature Alarm)**: Activate buzzer when temperature is strictly `< 18.0 °C` or `> 30.0 °C`.
- **FR-08 (Activity State)**: Support distinct ACTIVE and INACTIVE system states.
- **FR-09 (Automatic Inactivity)**: Transition to INACTIVE if no motion is detected for 15 seconds.
- **FR-10 (Automatic Reactivation)**: Restore ACTIVE state immediately upon PIR motion detection.

#### 1.3 Wokwi Simulation Adaptation
The physical target is simulated within the Wokwi embedded simulator using an ESP32 DevKit-C v4 board, DHT22 sensor, analog photoresistor module, digital PIR sensor, KY-040 rotary encoder, SSD1306 I2C OLED display, and piezo buzzer.

---

### 2. System Architecture and Design

#### 2.1 Hardware Architecture & Pin Mapping

| Peripheral | Component Interface | ESP32 GPIO | Electrical / Protocol Characteristics |
|---|---|---|---|
| **DHT22** | Single-Wire Serial Bus | `GPIO 4` | Bidirectional Open-Drain; microsecond timing handshake |
| **LDR Sensor** | Analog Output (AO) | `GPIO 34` | ADC1 Channel 6; 12-bit conversion (0–4095) |
| **PIR Sensor** | Digital Output (OUT) | `GPIO 13` | Digital Input; active-high pulse upon motion |
| **Rotary Encoder** | CLK (Phase A) | `GPIO 18` | Digital Input with internal pullup; quadrature signal |
| **Rotary Encoder** | DT (Phase B) | `GPIO 19` | Digital Input with internal pullup; quadrature signal |
| **Rotary Encoder** | SW (Pushbutton) | `GPIO 5` | Digital Input with internal pullup; active-low click |
| **SSD1306 OLED** | I2C Data (SDA) | `GPIO 21` | I2C Master Data (400 kHz bus) |
| **SSD1306 OLED** | I2C Clock (SCL) | `GPIO 22` | I2C Master Clock (400 kHz bus) |
| **Buzzer** | Signal (+) | `GPIO 23` | Digital Output; push-pull driver |

#### 2.2 Software Architecture and Subsystem Decomposition
The firmware codebase is partitioned into distinct functional modules:
- `sensors.*`: Encapsulates DHT22 protocol parsing, ADC sampling, and periodic data packaging.
- `display.*`: Manages SSD1306 I2C commands, page formatting, and exclusive display ownership.
- `input.*`: Decodes quadrature rotary transitions and executes page navigation logic.
- `alarm.*`: Implements temperature threshold evaluation and buzzer actuation.
- `motion.*`: Samples the PIR sensor and dispatches motion event flags.
- `system_state.*`: Tracks inactivity timers and coordinates ACTIVE/INACTIVE state transitions.
- `rtos_objects.*`: Centralizes FreeRTOS handles, queues, mutexes, and event group definitions.
- `main.cpp`: Acts strictly as the system entry point (`app_main`), orchestrating initialization and task launching.

#### 2.3 System State Machine
The system implements a two-state finite state machine (FSM):
- **ACTIVE**: OLED screen displays real-time metrics; rotary encoder navigation is live; sensor acquisition and thermal alarms are fully active.
- **INACTIVE**: Entered after 15 continuous seconds without motion. The OLED screen clears to a low-power sleep banner. Motion detection remains continuously active. Any PIR pulse immediately transitions the system back to ACTIVE.

---

### 3. FreeRTOS Architecture

#### 3.1 Concurrency Model and Task Table

The system features six concurrent FreeRTOS tasks:

| Task Name | Primary Responsibility | Trigger / Execution Period | Assigned Priority | IPC / Synchronization Primitive | Typical Blocked Condition |
|---|---|---|---|---|---|
| **`InputTask`** | Decodes encoder rotation; switches display mode | Short periodic (10 ms) | 3 | Direct memory update; UART mutex | Blocked on `vTaskDelay(10ms)` |
| **`MotionTask`** | Monitors PIR digital state; sets motion events | Short periodic (100 ms) | 3 | Event Group (`EVENT_MOTION`, `EVENT_ACTIVE`) | Blocked on `vTaskDelay(100ms)` |
| **`SensorTask`** | Samples DHT22 and LDR ADC; publishes sensor data | Fixed periodic (2000 ms) | 2 | Dedicated Queues (`xSensorQueueDisplay`, `xSensorQueueAlarm`) | Blocked on `vTaskDelayUntil()` |
| **`AlarmTask`** | Evaluates thermal limits; drives buzzer | Event-driven (new sensor reading) | 2 | Queue (`xSensorQueueAlarm`), Event Group (`EVENT_ALARM`) | Blocked on `xQueueReceive(portMAX_DELAY)` |
| **`StateTask`** | Monitors 15s inactivity; coordinates power states | Periodic cadence (1000 ms) | 2 | Event Group (`EVENT_ACTIVE`, `EVENT_MOTION`) | Blocked on `vTaskDelay(1000ms)` |
| **`DisplayTask`** | Owns and refreshes SSD1306 OLED screens | Event/Periodic (150–300 ms) | 1 | Queue (`xSensorQueueDisplay`), Event Group (`EVENT_ACTIVE`) | Blocked on `xQueueReceive(300ms)` |

#### 3.2 Technical Justification of Task Priorities
Task priorities in this design reflect **scheduling urgency and latency tolerance**, not functional importance:
1. **Priority 3 (InputTask, MotionTask)**: Quadrature rotary encoder pulses occur during manual user twisting. If not serviced within tens of milliseconds, pulses are missed, resulting in sluggish or skipped page switching. Similarly, motion detection requires prompt recognition. Because both tasks execute extremely brief computation (<100 microseconds) before yielding back to the scheduler, their high priority does not starve lower tasks.
2. **Priority 2 (SensorTask, AlarmTask, StateTask)**: Environmental physical variables change slowly; sampling every 2 seconds is more than adequate. However, safety evaluation (AlarmTask) and state management must not be unduly delayed by user-interface rendering.
3. **Priority 1 (DisplayTask)**: Human visual persistence tolerates latencies of 50–100 ms without degradation. Drawing frames across I2C takes significant CPU cycles. Giving DisplayTask the lowest priority ensures that slow display operations never block critical sensor sampling or user input detection.

#### 3.3 Periodic Scheduling with `vTaskDelayUntil()`
Section 22 and 23 of the syllabus mandate using `vTaskDelayUntil()` for periodic sensor sampling. 

Standard `vTaskDelay(pdMS_TO_TICKS(2000))` delays for 2000 ms *relative to the moment the function is called*. If sensor acquisition takes $\Delta t$ ms (e.g. 25 ms for DHT22 start pulses and bit reading), the actual period becomes $2000 + \Delta t$ ms. Over time, execution accumulates significant timing drift.

In contrast, `vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(2000))` calculates the delay interval *relative to the target scheduled wake tick*. If execution takes 25 ms, the task delays for exactly $2000 - 25 = 1975$ ms, maintaining a strict, non-drifting 2.000-second periodic schedule.

#### 3.4 Inter-Task Communication (Queue Fan-Out)
In FreeRTOS, `xQueueReceive()` removes the received item from the queue buffer. If `DisplayTask` and `AlarmTask` were to read from a single shared queue, a race condition would occur where whichever task woke first would consume the reading, starving the second task.

To resolve this, an explicit **fan-out queue pattern** was implemented:
- `SensorTask` produces a single `SensorData` struct.
- It posts the data independently to `xSensorQueueDisplay` and `xSensorQueueAlarm` via `xQueueOverwrite()`.
- Both consumer tasks reliably receive every update without interference.

#### 3.5 Resource Protection with Mutex
Multiple concurrent tasks print diagnostic logs to standard serial output (`stdout` over UART). Without mutual exclusion, character strings from different tasks interleave unpredictably (e.g., `[SensorTask] Te[AlarmTask] WARNmp: 24.5 C`).

`xSerialMutex` was implemented to wrap all serial output through `safeSerialPrintf()`. Any task seeking to write to UART must acquire `xSerialMutex` before writing and release it immediately afterward, guaranteeing thread-safe, atomic log messages.

#### 3.6 Event Group Signaling
A FreeRTOS Event Group (`xSystemEvents`) is utilized for lightweight, broadcast signaling:
- `BIT0 (EVENT_ACTIVE)`: Set by `StateTask` when the room is occupied; cleared after 15 seconds of inactivity. `DisplayTask` inspects this bit to decide whether to render active pages or blank the screen.
- `BIT1 (EVENT_MOTION)`: Set by `MotionTask` when the PIR pin is HIGH; cleared when LOW. Consumed by `StateTask` to reset the inactivity timer.
- `BIT2 (EVENT_ALARM)`: Set by `AlarmTask` when temperature is outside normal bounds; cleared when within limits. Consumed by `DisplayTask` to render visual warning banners.

---

### 4. Implementation Details

#### 4.1 Separation of Pure Decision Logic
A central engineering design decision was the strict separation of decision logic from hardware I/O:
- `AlarmState evaluateTemperature(float temperature)`: Evaluates thresholds independently of GPIO or PWM registers.
- `DisplayMode nextDisplayMode(DisplayMode current)` and `previousDisplayMode()`: Implements bidirectional state transitions and wraparound independently of encoder hardware.
- `SystemState evaluateSystemState(...)`: Implements timer and motion transition rules independently of FreeRTOS timers or PIR hardware.

This architecture enables unit testing on host environments without requiring hardware-in-the-loop simulation.

#### 4.2 Display Ownership
The SSD1306 OLED display is strictly owned by `DisplayTask`. No other task is permitted to issue I2C commands or write to the display buffer. Tasks communicate desired display information exclusively through FreeRTOS IPC primitives (the sensor queue and event group), eliminating bus contention and display corruption.

---

### 5. Verification and Testing

#### 5.1 Automated Unit Tests (13 Tests)
A total of 13 unit tests were implemented across three categories using the Unity test framework:

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

#### 5.2 Functional Verification Matrix in Wokwi

| Test ID | Stimulus / Action | Expected Result | Actual Observed Result | Status |
|---|---|---|---|---|
| **FT-01** | Modify DHT22 slider to 28.5 °C | OLED displays updated temperature | OLED page updates to `Temp: 28.5 C` within 2.0 seconds | **PASS** |
| **FT-02** | Modify DHT22 slider to 65.0 % | OLED displays updated humidity | OLED page updates to `Hum : 65.0 %` on humidity screen | **PASS** |
| **FT-03** | Adjust LDR ambient slider | Light percentage changes 0–100% | ADC raw value scales linearly to display percentage | **PASS** |
| **FT-04** | Twist rotary encoder clockwise | Cycles forward through pages | Sequentially selects Temp → Hum → Light → Motion → Temp | **PASS** |
| **FT-05** | Twist rotary encoder counterclockwise | Cycles backward through pages | Traverses in reverse order with complete wraparound | **PASS** |
| **FT-06** | Increase temperature to 35.0 °C (> 30 °C) | Buzzer triggers, alarm flagged | GPIO 23 turns HIGH, buzzer activates, OLED displays alarm banner | **PASS** |
| **FT-07** | Return temperature to 24.0 °C | Buzzer silences, alarm clears | GPIO 23 turns LOW, buzzer silences immediately | **PASS** |
| **FT-08** | Trigger PIR motion sensor | System is ACTIVE | `EVENT_MOTION` and `EVENT_ACTIVE` bits set | **PASS** |
| **FT-09** | Leave system idle for 15 seconds | System enters INACTIVE state | Screen clears to `SYSTEM SLEEP`, display refresh minimizes | **PASS** |
| **FT-10** | Trigger PIR motion while INACTIVE | System awakens to ACTIVE | Screen restores active metrics display immediately | **PASS** |

#### 5.3 Deliberate FreeRTOS Fault Experiments

##### Fault Experiment 1: Removal of Blocking Delay
- **Action**: In `SensorTask`, the blocking call `vTaskDelayUntil()` was temporarily commented out, leaving an unconstrained `while(1)` loop.
- **Observation**: Because `SensorTask` has Priority 2, it monopolized CPU Core 0. Lower-priority tasks on Core 0 (`StateTask`, `DisplayTask`) were starved of execution time. Furthermore, the FreeRTOS IDLE task on Core 0 was completely starved, preventing execution of background memory reclamation and triggering the ESP-IDF Task Watchdog Timer (TWDT) reset after 5 seconds.
- **Engineering Explanation**: FreeRTOS uses priority-based preemptive scheduling. A task that never blocks will run indefinitely, starving any task with equal or lower priority on the same core. All continuous tasks must periodically block, yield, or wait for synchronization primitives.

##### Fault Experiment 2: Unnecessary Priority Escalation
- **Action**: `DisplayTask` priority was raised from Priority 1 to Priority 4 (higher than `InputTask` and `MotionTask`).
- **Observation**: During complex OLED page rendering across I2C, rapid rotary encoder twists were dropped or experienced severe lag.
- **Engineering Explanation**: I2C transactions and frame rendering consume multiple milliseconds of CPU time. By assigning `DisplayTask` a higher priority than user input handling, the scheduler prioritized cosmetic screen rendering over mechanical edge capture. Restoring `InputTask` to Priority 3 and `DisplayTask` to Priority 1 restored instantaneous UI responsiveness.

##### Fault Experiment 3: Removal of Mutex Protection
- **Action**: The `xSemaphoreTake(xSerialMutex)` and `xSemaphoreGive(xSerialMutex)` wrappers were removed from `safeSerialPrintf()`.
- **Observation**: When `SensorTask` printed its periodic metric line simultaneously with `InputTask` reporting an encoder turn, the serial monitor printed mangled text: `[SensorTask] Temp: 2[InputTask] CW Rotation -> Display: LIGHT4.5 C`.
- **Engineering Explanation**: The UART hardware FIFO and standard I/O library buffers are non-atomic shared resources. Concurrent access causes character interleaving. The mutex enforces mutual exclusion, ensuring each log message completes atomically.

---

### 6. Static Code Analysis

Static analysis was executed using `pio check` with cppcheck:
- **Style / Unused Parameter Warning**: FreeRTOS task callbacks require a signature matching `void (*TaskFunction_t)(void *)`. In tasks not requiring external arguments, the compiler flagged `pvParameters` as unused. **Resolution**: Marked with `(void)pvParameters;` to explicitly acknowledge the standard API contract.
- **Data Conversion Warning**: Pointer conversion during I2C buffer transmission was flagged. **Resolution**: Enforced strict `const_cast` and explicit size checks.
- **Variable Linkage**: Internal helper functions in driver files were initially defined with external linkage. **Resolution**: Prefixed with `static` to restrict symbols to file translation scope.

---

### 7. Requirements Traceability Matrix

| Requirement ID | Description | Source Implementation File | Verification Evidence |
|---|---|---|---|
| **FR-01** | Periodic Temperature Measurement | `src/sensors.cpp` (`sensorTask`) | Unit tests (`test_alarm_*`), FT-01 in Wokwi |
| **FR-02** | Periodic Humidity Measurement | `src/sensors.cpp` (`sensorTask`) | FT-02 in Wokwi |
| **FR-03** | Relative Ambient Light Measurement | `src/sensors.cpp` (`readLDR`) | FT-03 in Wokwi |
| **FR-04** | PIR Motion Detection | `src/motion.cpp` (`motionTask`) | FT-08, FT-10 in Wokwi |
| **FR-05** | SSD1306 OLED Information Display | `src/display.cpp` (`displayTask`) | FT-01, FT-02, FT-03 in Wokwi |
| **FR-06** | Rotary Encoder Page Navigation | `src/input.cpp` (`inputTask`) | Unit tests (`test_navigation_*`), FT-04, FT-05 |
| **FR-07** | Temperature Buzzer Alarm | `src/alarm.cpp` (`alarmTask`) | Unit tests (`test_alarm_*`), FT-06, FT-07 |
| **FR-08** | ACTIVE / INACTIVE States | `src/system_state.cpp` (`stateTask`)| Unit tests (`test_state_*`), FT-08, FT-09 |
| **FR-09** | Automatic 15s Inactivity Timeout | `src/system_state.cpp` (`evaluateSystemState`) | Unit test `test_state_active_timeout`, FT-09 |
| **FR-10** | Automatic Reactivation on Motion | `src/motion.cpp`, `src/system_state.cpp` | Unit test `test_state_inactive_motion`, FT-10 |

---

### 8. Technical Defense Q&A Preparation

**1. Why did you create `SensorTask`?**  
To isolate time-consuming environmental sensor acquisition (specifically the microsecond-level 1-wire protocol of the DHT22 and ADC settling times) from user input handling and display rendering, ensuring deterministic sampling.

**2. Why does each task have its assigned priority?**  
Priorities reflect scheduling urgency. `InputTask` and `MotionTask` are Priority 3 to prevent missed user pulses; `SensorTask`, `AlarmTask`, and `StateTask` are Priority 2 for prompt environmental and safety evaluation; `DisplayTask` is Priority 1 because visual updates can tolerate minor latency without degrading user experience.

**3. What does `vTaskDelayUntil()` do?**  
It blocks the calling task until an absolute target tick count is reached, calculating the delay relative to the scheduled periodic interval rather than the time of function execution, eliminating timing drift.

**4. What happens to a task while it is delayed?**  
It transitions into the **Blocked** state, removed from the scheduler's Ready list. It consumes zero CPU cycles, allowing lower-priority tasks and the FreeRTOS IDLE task to execute.

**5. What information crosses your queue?**  
The `SensorData` struct, containing `float temperature`, `float humidity`, `int lightLevel`, and `bool motionDetected`.

**6. Why did you use a queue rather than unsynchronized global variables?**  
Queues provide thread-safe FIFO buffering with built-in blocking synchronization, preventing torn reads/writes when data is written by one task while being read by another.

**7. What resource does your mutex protect?**  
The shared standard serial output stream (`stdout` over ESP32 UART).

**8. Where could a race condition occur?**  
If multiple tasks call `printf` concurrently, output characters from different tasks interleave. In data passing, reading a global `SensorData` struct while `SensorTask` is halfway through updating it would produce a corrupted reading (torn read).

**9. What does your event group represent?**  
System-wide binary flags: `EVENT_ACTIVE (BIT0)` indicates active room operation; `EVENT_MOTION (BIT1)` indicates PIR activity; `EVENT_ALARM (BIT2)` indicates temperature threshold violation.

**10. Which task owns the OLED, and why?**  
`DisplayTask` exclusively owns the OLED to eliminate I2C bus contention and display buffer corruption that would occur if multiple tasks attempted to issue display commands.

**11. What happens if a high-priority task never blocks?**  
It enters an uncontrolled busy loop, starving all equal and lower-priority tasks on that core, starving the IDLE task, and eventually triggering a Task Watchdog Timer reset.

**12. What is the difference between Ready and Blocked?**  
A **Ready** task is able to execute immediately but is waiting for CPU scheduling because an equal or higher priority task is currently Running. A **Blocked** task cannot execute because it is waiting for an elapsed time interval or an external event/resource.

**13. What functionality did your unit tests actually verify?**  
They verified pure decision logic: all 5 boundary conditions for temperature alarms (<18, ==18, normal, ==30, >30), all 4 display navigation transitions and bidirectional wraparound, and all 4 state machine timeout/reactivation transitions.

**14. What did static analysis discover?**  
Unused task parameter declarations required by FreeRTOS signatures, and missing `static` linkage specifiers on internal driver helper functions.

**15. What would differ if this system ran on physical hardware?**  
Real hardware would introduce mechanical contact bounce on the rotary encoder requiring hardware debouncing capacitors or timer-based filtering, physical thermal inertia on the DHT22, non-linear optical response requiring lux calibration on the LDR, and real acoustic resonance characteristics on the piezo buzzer.

---

### 9. Conclusion

This laboratory activity successfully realized a robust, concurrent multisensor room monitoring system adhering strictly to native ESP-IDF and FreeRTOS engineering principles. By decomposing application responsibilities into six cooperating tasks, utilizing safe IPC primitives, and maintaining strict separation of hardware-independent decision logic, the resulting firmware is deterministic, testable, and maintainable. All acceptance criteria were satisfied and validated through automated unit testing, static code analysis, and interactive simulation in Wokwi.
