# STM32F4 Digital SWR & Power Meter (`swr-stm32f4-cmake`)

A high-performance, bare-metal embedded firmware for an RF Standing Wave Ratio (SWR) and RF Power Meter based on the **STM32F401xC** (ARM Cortex-M4) microcontroller. Built using CMake and GCC toolchain, directly driving peripherals via CMSIS registers without vendor HAL bloat for minimal footprint and maximum efficiency.

---

## 📊 Project Completion & Progress

| Category | Status | Progress |
| :--- | :---: | :---: |
| **Hardware Abstraction & Drivers (CMSIS)** | Ready | 100% |
| **Measurement & Calculation Engine** | Ready | 100% |
| **Build System & Toolchain Setup** | Ready | 100% |
| **UI & Display Subsystem** | Functional | 85% |
| **Menu & Button Navigation** | Functional | 75% |
| **Interactive Calibration Storage** | In Progress | 25% |
| **Hardware Field Testing & Fine-Tuning** | Pending | 10% |
| **Overall Project Completion** | **Active Development** | **~70%** |

### Detailed Checklist

- [x] **Core Drivers (Register-level / Bare-Metal)**
  - [x] SysTick timer driver for microsecond/millisecond delays (`sytick.c`)
  - [x] I2C1 Master driver with open-drain GPIO configuration (`lcd.c`)
  - [x] HD44780 character LCD driver via PCF8574 backpack (`lcd.c`)
  - [x] 12-bit ADC1 driver for PA0 (CH0), PA1 (CH1), and PA2 (CH2) (`adc.c`)
  - [x] Active-low GPIO button input driver with internal pull-ups (`button.c`)
- [x] **Calculations & DSP**
  - [x] Fast integer square root algorithm (`isqrt`)
  - [x] SWR computation with edge-case protection (infinite SWR, zero forward power, perfect match)
  - [x] Multi-point Piecewise Linear Interpolation (PWLI) for diode non-linearity correction (`conversion.c`)
  - [x] Default 7-point calibration lookup curves for FWD, REF, and RAD channels
- [x] **Display & UI Navigation**
  - [x] Main measurement screen displaying Forward Power, SWR, and Reflected/Radiated Power
  - [x] Refresh optimization (only updates screen when sensor values or button states change)
  - [x] 3-Button navigation state machine (Up / Select / Down)
  - [x] Display toggle between Reflected (REF) and Radiated (RAD) views (`Menu UI`)
  - [x] Calibration sub-menu navigation skeleton (`Cal FWD`, `Cal REF`, `Cal RAD`)
- [ ] **Interactive In-System Calibration (Current Work)**
  - [ ] Live calibration point capture ("Add Cal Point")
  - [ ] Calibration point removal / editing via UI ("Remove Cal Point")
  - [ ] Non-volatile persistence (Flash memory sector emulation)
- [ ] **Enhancements & System Polish**
  - [ ] Button software debouncing and long-press support
  - [ ] Peak Envelope Power (PEP) hold and decay filter
  - [ ] RF high-SWR warning alarm / LED alert trigger (PC13)
  - [ ] Live RF power bench calibration with dummy load

---

## 🛠️ Hardware Specification & Pinout

Target Board: **STM32F401CCU6 / STM32F401xC** ("BlackPill", 84 MHz ARM Cortex-M4)

| Peripheral / Signal | Pin | Direction | Description |
| :--- | :--- | :--- | :--- |
| **FWD Voltage** | `PA0` (ADC1_IN0) | Input (Analog) | Forward RF power detector voltage (0 – 3.3V) |
| **REF Voltage** | `PA1` (ADC1_IN1) | Input (Analog) | Reflected RF power detector voltage (0 – 3.3V) |
| **RAD Voltage** | `PA2` (ADC1_IN2) | Input (Analog) | Radiated RF power / auxiliary sensor voltage (0 – 3.3V) |
| **I2C1 SCL** | `PB6` | Output (AF4, OD) | I2C Clock for PCF8574 LCD backpack |
| **I2C1 SDA** | `PB7` | Bidirectional (AF4, OD) | I2C Data for PCF8574 LCD backpack |
| **Button UP** | `PB13` | Input (Pull-Up) | Menu navigation: Previous / Next / Increment (Active Low) |
| **Button SELECT** | `PB14` | Input (Pull-Up) | Menu navigation: Enter / Confirm (Active Low) |
| **Button DOWN** | `PB15` | Input (Pull-Up) | Menu navigation: Next / Decrement (Active Low) |
| **Status LED** | `PC13` | Output (Push-Pull) | Onboard status indicator LED |

> [!NOTE]
> The RF directional coupler or diode detectors (e.g., AD8307, AD8318, or 1N5711 Schottky diodes) should condition RF voltages to within the ADC safe input range of **0 – 3.3V**.

---

## 📐 Mathematical Formulation

### 1. Piecewise Linear Interpolation
Sensor detector curves are non-linear across their dynamic range. The firmware applies piecewise linear interpolation across $N$ predefined calibration nodes:

$$P = Y_0 + \frac{(ADC_{raw} - X_0) \times (Y_1 - Y_0)}{X_1 - X_0}$$

### 2. SWR Computation
SWR is determined from forward power ($P_{fwd}$) and reflected power ($P_{ref}$) via an integer square root algorithm to extract relative voltages without floating-point penalty:

$$SWR = \frac{\sqrt{P_{fwd}} + \sqrt{P_{ref}}}{\sqrt{P_{fwd}} - \sqrt{P_{ref}}}$$

- $P_{fwd} \le P_{ref}$: Clipped to `9.99` (Total reflection / Open / Short)
- $P_{fwd} == 0$: Returns `9.99`
- $P_{ref} == 0$: Returns `1.00` (Perfect match)

---

## 🗂️ Project Structure

```text
swr-stm32f4-cmake/
├── CMakeLists.txt              # CMake project definition & build targets
├── arm-none-eabi.cmake         # Cross-compilation toolchain file
├── LinkerScript.ld             # Memory map and linker script for STM32F401xC
├── README.md                   # Project documentation & status
├── Core/
│   ├── Inc/                    # Header files
│   │   ├── adc.h               # ADC driver definitions
│   │   ├── button.h            # Button input definitions
│   │   ├── conversion.h        # Interpolation & SWR math definitions
│   │   ├── lcd.h               # I2C HD44780 LCD driver definitions
│   │   ├── menu.h              # Menu state machine & UI definitions
│   │   └── sytick.h            # SysTick delay definitions
│   └── Src/                    # Implementation files
│       ├── adc.c               # ADC1 register-level multi-channel driver
│       ├── button.c            # GPIO button handling
│       ├── conversion.c        # SWR calculation, integer sqrt, lookup tables
│       ├── lcd.c               # I2C1 and HD44780 controller logic
│       ├── main.c              # Application entry point and main loop
│       ├── menu.c              # UI menu hierarchy and rendering logic
│       ├── startup_stm32f401xc.s # Vector table and reset handler
│       ├── system_stm32f4xx.c  # CMSIS clock and system initialization
│       └── sytick.c            # Delay utilities (delay_ms, delay_us)
└── Drivers/
    └── CMSIS/                  # ARM Cortex-M and STM32 CMSIS headers
```

---

## ⚙️ Building and Flashing

### Prerequisites

Ensure you have the following installed on your host machine:

- **ARM GNU Toolchain**: `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`
- **CMake** (v3.20 or newer)
- **Ninja**
- **ST-Link Tools** (`stlink-tools` / `st-flash`) or OpenOCD

### 1. Configure the Project

```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
```

### 2. Compile

```bash
ninja -C build
```

This generates `swr-stm32f4-cmake.elf`, `swr-stm32f4-cmake.bin`, and `swr-stm32f4-cmake.hex` inside the `build/` directory with size statistics printed automatically.

### 3. Flash to Microcontroller

Using an ST-Link V2 programmer:

```bash
ninja -C build flash
```

Or manually with `st-flash`:

```bash
st-flash --reset write build/swr-stm32f4-cmake.bin 0x08000000
```

---

## 📄 License

This project is licensed under the **PolyForm Noncommercial License 1.0.0**.

- **Permitted**: Personal experimentation, hobbyist/amateur radio usage, educational study, academic research, and non-commercial development.
- **Prohibited**: Any commercial use, including selling, embedding into commercial products or hardware, or using for monetary compensation, without explicit written permission from the author.

See the full [LICENSE](file:///home/syaiful/Projects/swr-stm32f4-cmake/LICENSE) file for terms.
