# Digital RF SWR & Power Meter (`swr-stm32f4-cmake`)

A high-performance embedded firmware and hardware project for an RF Standing Wave Ratio (SWR) and RF Power Meter. 

> [!IMPORTANT]
> **Project Architecture & Porting Strategy:**
> - **Prototyping Platform (Current)**: **STM32F401xC** (ARM Cortex-M4 @ 84 MHz) used for rapid firmware development, testing arithmetic algorithms, menu state machines, and hardware protection logic.
> - **Target Production Platform**: **STC8H8K64U-45I-LQFP32** (High-speed 1T 8051 MCU with 64 KB Flash, 8 KB RAM, and 12-bit ADC).
> - **Why CMake?**: The project is structured with modern **CMake** to maintain a clean separation between architecture-independent business logic (SWR integer math, piecewise linear interpolation, menu hierarchies) and low-level peripheral drivers. This modularity ensures seamless portability when transitioning from the ARM GCC toolchain to SDCC / Keil C51 for the STC8.

---

## 📊 Project Completion & Progress

| Category | Status | Progress |
| :--- | :---: | :---: |
| **STM32F4 Peripheral Drivers (CMSIS)** | Ready | 100% |
| **Measurement & SWR Calculation Engine** | Ready | 100% |
| **CMake Build System & Toolchain Setup** | Ready | 100% |
| **UI & Display Subsystem (Prototyping)** | Functional | 90% |
| **Menu & Button Navigation State Machine** | Functional | 100% |
| **Interactive In-Flash Calibration Storage** | Ready | 100% |
| **Hardware Protection (Relay Cutoff & Buzzer)** | In Development | 30% |
| **Digital Controller PCB & Schematic (STC8)** | **Ready (Gerbers Generated)** | **100%** |
| **Physical PCB Fabrication & Assembly** | Pending | 0% |
| **STC8 Target Porting (STC8H8K64U)** | Planned (Post-STM32 verification) | 0% |
| **Hardware Field Testing & RF Bench Tuning** | Pending | 10% |
| **Overall Project Completion** | **Active Development** | **~80%** |

### Detailed Checklist

- [x] **Core Drivers (STM32 Prototyping - Register Level)**
  - [x] SysTick timer driver for microsecond/millisecond delays (`sytick.c`)
  - [x] I2C1 Master driver with open-drain GPIO configuration (`lcd.c`)
  - [x] HD44780 character LCD driver via PCF8574 backpack (`lcd.c`)
  - [x] 12-bit ADC1 driver for PA0 (CH0), PA1 (CH1), and PA2 (CH2) (`adc.c`)
  - [x] Active-low GPIO button input driver with debouncing and auto-repeat (`button.c`)
- [x] **Calculations & DSP (Portable C)**
  - [x] Fast integer square root algorithm (`isqrt`)
  - [x] SWR computation with edge-case protection (infinite SWR, zero forward power, perfect match)
  - [x] Multi-point Piecewise Linear Interpolation (PWLI) for diode non-linearity correction (`conversion.c`)
  - [x] Default 7-point calibration lookup curves for FWD (1000W), REF (100W), and RAD (50W)
- [x] **Display & UI Navigation**
  - [x] Main measurement screen displaying Forward Power, SWR, and Reflected/Radiated Power
  - [x] Refresh optimization (only updates screen when sensor values or button states change)
  - [x] 3-Button debounced navigation state machine (Up / Select / Down)
  - [x] Display toggle between Reflected (REF) and Radio-In (RAD) views (`Menu UI` & quick toggle)
  - [x] Full calibration navigation workflows for FWD, REF, and RAD channels
- [x] **Interactive In-System Calibration & Storage**
  - [x] Dynamic calibration point management (Add Point, Edit Point, Remove Point, View Points)
  - [x] Dynamic capacity up to 10 points per channel with minimum 2-point safety guard
  - [x] Target wattage adjustment with dynamic scaling (FWD up to 1000W, REF up to 100W, RAD up to 50W)
  - [x] 5-second sampling routine with live countdown and continuous ADC averaging
  - [x] Automatic monotonic table sorting upon calibration point updates
  - [x] Flash non-volatile persistence (Sector 5 storage HAL with checksum verification)
  - [x] Calibration point table inspection view (`View Points`) and factory default reset
- [ ] **Hardware Protection & Safety (Relay & Buzzer)**
  - [ ] High-SWR protection relay trigger (disconnect transmitter / PTT line when SWR exceeds safety limit, e.g. $\text{SWR} > 3.0$)
  - [ ] Optocoupler-isolated relay latching and fault lockout logic
  - [ ] Audible alarm tone via active buzzer on high SWR or over-power condition
  - [ ] On-screen fault warning message with manual/auto reset mechanisms
- [x] **Custom Digital Controller PCB & Schematic (STC8)**
  - [x] Schematic capture V1.0 completed in EasyEDA ([`SCH_Schematic1_2026-09-30.pdf`](PCB%20and%20Schematic/SCH_Schematic1_2026-09-30.pdf))
  - [x] 4-Layer PCB layout completed in EasyEDA Pro (~$117.5 \times 36\,\text{mm}$)
  - [x] Production Gerber and drill file generation ([`PCB and Schematic/Gerber/`](PCB%20and%20Schematic/Gerber/))
  - [x] Optocoupler-isolated relay driver (PC817C + BC547 + SRD-05VDC-SL-C)
  - [x] Analog input protection (voltage dividers $10\text{k}\Omega / 3.3\text{k}\Omega$, $10\text{nF}$ filter capacitors, `SMAJ5.0A` TVS diodes)
  - [x] Dual power input (7805 regulator + Micro USB with ESD diodes)
  - [ ] Order PCB manufacturing and assemble physical components
- [ ] **STC8 Migration & Porting**
  - [ ] Set up SDCC / Keil C51 toolchain (integrating with CMake)
  - [ ] Write STC8H8K64U register-level drivers (ADC, Timer, Direct 4-bit LCD, GPIO, Flash IAP)
  - [ ] Port business logic, calibration tables, and protection routines to STC8

---

## 🎛️ Digital Hardware Architecture & Schematic Overview

The custom PCB design focuses exclusively on the **Digital Controller Board**. The RF directional coupler and detector stage (e.g., tandem-match coupler with Schottky/AD8307 detectors) are kept external and feed DC voltages into the digital board via screw terminals (`KF301`).

```
                +----------------------------------------------------+
                |              DIGITAL CONTROLLER BOARD              |
                |                                                    |
[ External RF ] |  [ Screw Terminals ]                               |
[   Coupler   ]==> [ FWD / REF / RAD ]                               |
                |          |                                         |
                |  [ TVS + Divider ]                                 |
                |  (SMAJ5.0A + 10k/3.3k)                             |
                |          |                                         |
                |          v                                         |
                |    +------------+        +----------------------+  |
                |    |  MCU Core  |=======>| 1602 LCD Display     |  |
                |    |            |        +----------------------+  |
                |    |  STM32F4   |=======>| 3x Navigation Buttons|  |
                |    |   (Proto)  |        +----------------------+  |
                |    |     |      |=======>| Piezo Buzzer         |  |
                |    |     v      |        +----------------------+  |
                |    |  STC8H8K   |        +----------------------+  |
                |    |  (Target)  |=======>| Opto-Isolated Relay  |===> [ Transmitter PTT / ]
                |    +------------+        | (PC817C + BC547)     |     [ RF Protection Cutoff ]
                |                          +----------------------+  |
                +----------------------------------------------------+
```

### Digital Board Subsystems & Hardware Artifacts

The schematic and 4-layer PCB layout for the target STC8 board are fully designed and exported:
- 📄 **Schematic PDF**: [`PCB and Schematic/SCH_Schematic1_2026-09-30.pdf`](PCB%20and%20Schematic/SCH_Schematic1_2026-09-30.pdf)
- 📦 **Fabrication Gerbers**: [`PCB and Schematic/Gerber/`](PCB%20and%20Schematic/Gerber/) (4-layer stackup: `GTL`, `G1` GND plane, `G2` PWR plane, `GBL`, with drills and solder masks, dimensions: ~$117.5 \times 36\,\text{mm}$)

#### Circuit Subsystems (From EasyEDA Schematic V1.0)
1. **Main Microcontroller**: `STC8H8K64U-45I-LQFP32` (Target Production MCU)
2. **Power Supply Section**:
   - Primary: DC jack/terminal (`KF301-5.0-2P`) with 1N4007 reverse polarity diode into an onboard **LM7805** regulator with $100\,\mu\text{F}$ filter capacitors.
   - Secondary / Programming: **Micro USB** port with `SMAJ5.0A` TVS diode ESD protection and UART TX/RX debug pins.
3. **ADC Conditioning & Protection**:
   - Dedicated screw terminals for `FWD(IN)`, `REF(IN)`, and `R-IN(IN)`.
   - $10\,\text{k}\Omega / 3.3\,\text{k}\Omega$ precision voltage dividers scaling down high detector voltages.
   - $10\,\text{nF}$ ceramic bypass capacitors for RF decoupling.
   - `SMAJ5.0A` TVS clamp diodes protecting MCU ADC pins against voltage spikes.
4. **Relay Protection Circuit**:
   - Optoisolated interface using a **PC817C** optocoupler to isolate MCU digital ground from relay coil noise.
   - **BC547** NPN transistor switching an **SRD-05VDC-SL-C** 5V power relay.
   - **1N4007** flyback diode across the relay coil.
   - Screw terminals (`KF301-5.0-3P`) providing normally open/closed contacts to safely disconnect transmitter PTT or engage RF attenuator during high-SWR faults.
5. **Human-Machine Interface (HMI)**:
   - **LCD 1602** (HS1602A-B) character display driven via 4-bit parallel bus (`RS`, `EN`, `DAT4`–`DAT7`) with contrast potentiometer (`PR1 10k`).
   - 3x tactile pushbuttons (`TS1103S` SMD) for intuitive menu navigation.
   - Active audible buzzer for immediate alarm alerts.

---

## 📌 Pinout & Hardware Mapping

### Prototyping Board (STM32F401xC) vs. Final Target Board (STC8H8K64U)

| Function / Signal | STM32F401xC Pin (Proto) | STC8H8K64U Pin (Target PCB) | Description |
| :--- | :--- | :--- | :--- |
| **FWD ADC Input** | `PA0` (ADC1_IN0) | `P1.0` (ADC0) | Forward RF voltage via $10\text{k}/3.3\text{k}$ divider |
| **REF ADC Input** | `PA1` (ADC1_IN1) | `P1.1` (ADC1) | Reflected RF voltage via $10\text{k}/3.3\text{k}$ divider |
| **RAD ADC Input** | `PA2` (ADC1_IN2) | `P1.4` (ADC4) | Radiated / auxiliary RF voltage via divider |
| **Button UP / 1** | `PB13` (Pull-Up) | `P0.1` (BTN1) | Menu Up / Increment / Cycle (Active Low) |
| **Button SELECT / 2** | `PB14` (Pull-Up) | `P0.2` (BTN2) | Menu Enter / Confirm / Select (Active Low) |
| **Button DOWN / 3** | `PB15` (Pull-Up) | `P0.3` (BTN3) | Menu Down / Decrement / Cycle (Active Low) |
| **Relay Control** | GPIO Output (Pending) | `P2.7` | Controls PC817C optocoupler $\rightarrow$ Relay |
| **Alarm Buzzer** | GPIO Output (Pending) | `P2.6` | High-SWR audible alarm buzzer |
| **LCD RS** | (I2C Backpack in Proto) | `P2.5` | 1602 LCD Register Select |
| **LCD EN** | (I2C Backpack in Proto) | `P2.4` | 1602 LCD Enable Strobe |
| **LCD DAT4** | (I2C Backpack in Proto) | `P2.3` | 1602 LCD Data Bit 4 |
| **LCD DAT5** | (I2C Backpack in Proto) | `P2.2` | 1602 LCD Data Bit 5 |
| **LCD DAT6** | (I2C Backpack in Proto) | `P2.1` | 1602 LCD Data Bit 6 |
| **LCD DAT7** | (I2C Backpack in Proto) | `P2.0` | 1602 LCD Data Bit 7 |
| **Status LED** | `PC13` (Onboard LED) | — | Prototyping visual status indicator |

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
├── arm-none-eabi.cmake         # Cross-compilation toolchain file for STM32
├── LinkerScript.ld             # Memory map and linker script for STM32F401xC
├── README.md                   # Project documentation, roadmap & hardware guide
├── LICENSE                     # PolyForm Noncommercial License 1.0.0
├── PCB and Schematic/          # Target hardware artifacts (STC8)
│   ├── SCH_Schematic1_2026-09-30.pdf # Complete digital schematic V1.0
│   └── Gerber/                 # Production 4-layer Gerber & drill files
│       ├── Gerber_TopLayer.GTL
│       ├── Gerber_InnerLayer1.G1   # GND Plane
│       ├── Gerber_InnerLayer2.G2   # PWR Plane
│       ├── Gerber_BottomLayer.GBL
│       ├── Drill_PTH_Through.DRL
│       └── ...
├── Core/
│   ├── Inc/                    # Header files
│   │   ├── adc.h               # ADC driver definitions
│   │   ├── button.h            # Button input definitions
│   │   ├── conversion.h        # Interpolation & SWR math definitions (portable)
│   │   ├── lcd.h               # LCD driver definitions
│   │   ├── menu.h              # Menu state machine definitions (portable)
│   │   └── sytick.h            # SysTick delay definitions
│   └── Src/                    # Implementation files
│       ├── adc.c               # ADC1 register-level multi-channel driver
│       ├── button.c            # GPIO button handling
│       ├── conversion.c        # SWR calculation, integer sqrt, lookup tables
│       ├── lcd.c               # LCD interface logic
│       ├── main.c              # Application entry point and main loop
│       ├── menu.c              # UI menu hierarchy and rendering logic
│       ├── startup_stm32f401xc.s # Vector table and reset handler
│       ├── system_stm32f4xx.c  # CMSIS clock and system initialization
│       └── sytick.c            # Delay utilities (delay_ms, delay_us)
└── Drivers/
    └── CMSIS/                  # ARM Cortex-M and STM32 CMSIS headers
```

---

## ⚙️ Building and Flashing (STM32 Prototyping)

### Prerequisites

- **ARM GNU Toolchain**: `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`
- **CMake** (v3.20 or newer)
- **Ninja** or **GNU Make**
- **ST-Link Tools** (`stlink-tools` / `st-flash`)

### 1. Configure the Project

```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
```

### 2. Compile

```bash
cmake --build build
# or: ninja -C build
```

### 3. Flash to Microcontroller

```bash
cmake --build build --target flash
# or: st-flash --reset write build/swr-stm32f4-cmake.bin 0x08000000
```

---

## 📄 License

This project is licensed under the **PolyForm Noncommercial License 1.0.0**.

- **Permitted**: Personal experimentation, hobbyist/amateur radio usage, educational study, academic research, and non-commercial development.
- **Prohibited**: Any commercial use, including selling, embedding into commercial products or hardware, or using for monetary compensation, without explicit written permission from the author.

See the full [LICENSE](LICENSE) file for terms.
