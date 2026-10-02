# Digital SWR & Power Meter

Digital SWR and RF power meter designed primarily for monitoring VHF power amplifiers / boosters (144 MHz / 2m band).

The firmware is currently running on an **STM32F401xC** ("Black Pill") for prototyping and algorithm tuning. The final target hardware is based on an **STC8H8K64U** microcontroller; schematic, 4-layer Gerber files, and PCB render images are available in `PCB and Schematic/`. All core math, calibration, menu logic, and LCD rendering are written in portable C to make porting to 8051 (SDCC / Keil C51) straightforward.

---

## Hardware & PCB Design

The custom digital controller PCB is designed around the **STC8H8K64U-45I-LQFP32** with direct 4-bit parallel LCD interface, active buzzer, optocoupler-driven relay cutoff, and TVS clamp protection on ADC inputs.

| View | With 3D Components | 2D PCB Layout |
| :--- | :---: | :---: |
| **Front** | ![PCB Front 3D](<PCB and Schematic/front.jpeg>) | ![PCB Front 2D](<PCB and Schematic/front no 3d.jpeg>) |
| **Back** | ![PCB Back 3D](<PCB and Schematic/back.jpeg>) | ![PCB Back 2D](<PCB and Schematic/back no 3d.jpeg>) |

> **Why is there an anime graphic on the PCB silkscreen?**  
> The reason is: *Anime*.

---

## Features

- **RF Measurements**: 3-channel 12-bit ADC sensing for Forward (`FWD`), Reflected (`REF`), and Radio Drive (`RAD`) voltages.
- **Diode Non-Linearity Compensation**: Piecewise linear interpolation (PWLI) with up to 10 user-defined calibration points per channel.
- **Proportional Curve Adaptation**: Calibrating a single known power reference (e.g. 20W) prompts `Adapt all: >YES / >NO` to proportionally scale the entire curve, preserving diode non-linearity while matching coupler sensitivity.
- **Monotonicity Protection**: Automatically detects and prevents inverted calibration points ($W_1 < W_2$ but $ADC_1 \ge ADC_2$) to prevent negative slopes or calculation bugs.
- **Auto-Marquee Scrolling**: Any LCD line exceeding 16 characters automatically scrolls smoothly (1000 ms pause, 250 ms shift per char, wrap-around) with smart frame-caching to eliminate LCD flicker.
- **Fast Integer Math**: SWR and power calculations use pure integer arithmetic and a fast integer square root (`isqrt`), keeping execution fast without floating-point overhead.
- **Signal Filtering**: Exponential moving average (EMA) filter on ADC readings to smooth out mains noise and RF jitter without introducing noticeable lag.
- **Tuned Display Refresh**: Main screen updates every 180 ms (~5.5 Hz) to match standard HD44780 LCD response times and prevent ghosting.
- **Power Bar Options**:
  - Custom tick scale (`''''|`) using 8 custom LCD CGRAM glyphs (1px ticks, half-ticks, full division marks).
  - Solid progressive bar (`|||||`).
  - Off (shows REF or RAD numeric wattage on line 2).
  - Quick toggle using UP / DOWN buttons on the home screen.
- **Hardware Protection (2-State Architecture)**:
  - High SWR cutoff (configurable from 1.1 to 5.0, only trips when FWD power is present).
  - Over-drive cutoff (RAD power limit from 1W to 50W).
  - Drives an optocoupler + relay (`PB2`) to drop transmitter PTT or insert an attenuator on fault.
  - Active buzzer alarm on trip (PB0 on proto, P2.6 on STC8).
  - **Auto-protect & non-softlock navigation**: Overload immediately cuts off the relay and switches to the trip screen (`*TRIP* HI SWR!` or `*TRIP* HI RAD!`). Pressing SELECT resets protection; holding SELECT (500ms) or UP / DOWN enters the menu without being kicked out or softlocked. Relay remains safely latched off while in the menu.
  - Manual relay test toggle in menu.
- **On-Device Calibration**:
  - Add, edit, remove, and view calibration points directly from the LCD menu.
  - Re-calibrating an existing wattage updates that point in place. Points are automatically sorted by raw ADC value.
  - Settings and calibration curves saved to Flash Sector 5 with checksum validation.
  - Unsaved change prompt on exit, plus dynamic `> Save Config` menu entry when changes are pending in RAM.
- **Standby Screen Saver**:
  - After 30 seconds of idle power (0W) without button presses, Line 2 scrolls a status marquee showing max power limits and protection status.
  - Exits immediately on any button press or RF transmission (>1W).
  - Can be toggled on/off in the menu (`> Standby Text`).
- **Hardware Watchdog with Crash Breadcrumbs**:
  - Hardware IWDG recovers automatically if RF/EMI causes CPU lockup.
  - Subsystem checkpoints stored in persistent RAM (`.noinit`) identify the exact cause upon reboot (e.g. `Why: I2C Bus Lock`, `Why: ADC Read Hang`, `Why: Manual Test`, `Why: Main Loop Stall`).
  - Notifies user on LCD at startup if reboot was caused by watchdog.
- **Hardware Diagnostics Menu**:
  - Live raw ADC monitor for FWD, REF, and RAD channels.
  - Real-time GPIO button monitor (`UP`, `SEL`, `DN`).
  - Manual relay and buzzer toggles.
  - Boot reset reason and intentional watchdog reset test.
- **Symmetrical 25ms Debounced Buttons**:
  - Industrial-grade debounce filter eliminates tactile switch chatter on both press and release.
  - Single-consumption event flags prevent double-clicks.
  - Menu transitions automatically disarm pending releases, preventing keypress bleed across screens.
  - 500 ms long hold for alternate actions.

---

## Project Status & Roadmap

| Item | Platform | Status |
| :--- | :--- | :--- |
| Core drivers (ADC, I2C, SysTick, GPIO, Buzzer) | STM32F401xC | Working |
| SWR math, PWLI interpolation, EMA filter | Portable C | Working |
| HD44780 LCD driver & custom bar characters | Portable C | Working |
| Button debouncing & long-press handling | STM32 / Portable | Working |
| Multi-point calibration, curve adapt & Flash storage | Portable C / STM32 | Working |
| Protection limits, relay cutoff & buzzer alarm | Portable C | Working |
| Display modes (Scale bar, Pipe bar, Text) | Portable C | Working |
| Standby marquee screensaver | Portable C | Working |
| Hardware IWDG watchdog with crash breadcrumbs | STM32F401xC | Working |
| Hardware diagnostic & self-test suite | Portable C | Working |
| Modular menu architecture (screen, cal, prot, diag) | Portable C | Working |
| STC8 PCB schematic, 4-layer Gerbers & 3D renders | Hardware | Complete (`PCB and Schematic/`) |
| Physical board assembly | Hardware | In progress |
| Port to STC8H8K64U | STC8 (8051) | Planned |

### Next Steps / TODO

- [ ] **RF Bench Testing with VHF Booster**: Test the prototype with a real 2m VHF booster into a dummy load. Testing will be done first on a prototype detector board without TVS diodes installed to check raw diode response and calculation linearity. Results and calibration tables will be updated after testing.
- [ ] **Protection Relay Test**: Verify trip response time during high SWR (mismatched load) and over-drive conditions under actual RF power.
- [ ] **Port to STC8H8K64U**: Once ordered parts and PCBs arrive, port the code to STC8 (using SDCC / Keil). The core math (SWR calculation, interpolation, moving average filter, menu engine, LCD bar graphics) is written in portable C to make porting straightforward.

---

## Pinout

| Signal | STM32F401 (Proto) | STC8H8K64U (Target PCB) | Description |
| :--- | :--- | :--- | :--- |
| **FWD ADC** | `PA0` (ADC1_IN0) | `P1.0` (ADC0) | Forward RF voltage via divider |
| **REF ADC** | `PA1` (ADC1_IN1) | `P1.1` (ADC1) | Reflected RF voltage via divider |
| **RAD ADC** | `PA2` (ADC1_IN2) | `P1.4` (ADC4) | Radio drive RF voltage via divider |
| **Button UP** | `PB13` (Pull-up) | `P0.1` | Up / increment / view cycle (active low) |
| **Button SELECT** | `PB14` (Pull-up) | `P0.2` | Select / confirm / hold: menu (active low) |
| **Button DOWN** | `PB15` (Pull-up) | `P0.3` | Down / decrement / view cycle (active low) |
| **Relay** | `PB2` | `P2.7` | Drives optocoupler (High = normal, Low = tripped) |
| **Buzzer** | `PB0` | `P2.6` | Buzzer on trip (alarm) |
| **I2C SCL** | `PB6` | — | LCD backpack clock (proto) |
| **I2C SDA** | `PB7` | — | LCD backpack data (proto) |
| **LCD Parallel** | — | `P2.0`–`P2.5` | 4-bit parallel bus (target PCB) |
| **Status LED** | `PC13` | — | Onboard LED heartbeat (proto) |

---

## Menu Structure

```
[ Main Screen ]
  Line 0: FWD: %4uW S:%u.%02u
  Line 1: [Scale Bar] | [Pipe Bar] | [REF / RAD text]
  UP / DOWN: Cycle display view
  SELECT: Open main menu

  ├── [ > Save Config ] (Only visible when unsaved changes exist in RAM)
  │     SEL: Write changes to flash
  │
  ├── [ > Cal FWD ] / [ > Cal REF ] / [ > Cal RAD ]
  │     ├── 1. Add Point    -> Target Watts -> Apply RF -> 5s Sample -> Adapt all? -> Save
  │     ├── 2. Edit Point   -> Pick point -> Edit Watts -> Sample -> Save
  │     ├── 3. Remove Point -> Pick point -> Confirm delete
  │     ├── 4. View Points  -> Browse saved calibration table
  │     ├── 5. Save Flash   -> Commit channel points to flash
  │     ├── 6. Reset Def    -> Restore default curves
  │     └── 7. Back         -> Return to main menu (prompts if unsaved)
  │
  ├── [ > Protections ]
  │     ├── 1. Protection ON/OFF -> Master protection toggle
  │     ├── 2. Set RAD Limit     -> 1W to 50W (or OFF)
  │     ├── 3. Set SWR Limit     -> 1.1 to 5.0 (or OFF)
  │     ├── 4. Relay Test        -> Toggle relay on/off manually
  │     ├── 5. Reset Trip        -> Clear trip condition
  │     ├── 6. Save Flash        -> Commit protection settings to flash
  │     └── 7. Back              -> Return to main menu
  │
  ├── [ > Display Mode ]
  │     ├── Power Bar Mode: OFF
  │     ├── Power Bar Mode: Custom Scale (''''|)
  │     └── Power Bar Mode: Solid Pipes (|||||)
  │           SEL: Apply mode | Hold SEL (500ms): Return to main menu
  │
  ├── [ > Standby Text ]
  │     SEL: Toggle standby screen saver ON / OFF
  │
  ├── [ > Diagnostic ]
  │     ├── 1. Live ADC     -> Real-time raw ADC monitor (FWD, REF, RAD)
  │     ├── 2. Buttons Test -> Real-time button GPIO monitor (UP, SEL, DN)
  │     ├── 3. Relay Test   -> Toggle relay state on PB2
  │     ├── 4. Buzzer Test  -> Toggle buzzer output on PB0
  │     ├── 5. Boot Reason  -> Displays reset cause & crash breadcrumb
  │     ├── 6. Watchdog Test-> Halt CPU to test watchdog reboot
  │     └── 7. Back         -> Return to main menu
  │
  └── [ > Back to Main ]
        SEL: Return to home screen
```

---

## Calculations

### Piecewise Linear Interpolation

Detector diodes are non-linear, especially at lower power levels. The code interpolates between calibrated points:

$$P = Y_0 + \frac{(ADC_{\text{raw}} - X_0) \times (Y_1 - Y_0)}{X_1 - X_0}$$

Where $(X_0, Y_0)$ and $(X_1, Y_1)$ are adjacent calibration points (raw ADC value vs actual power in Watts).

### SWR Calculation

SWR is computed from forward and reflected power using an integer square root routine without floating-point math:

$$\Gamma \times 1000 = \sqrt{\frac{P_{\text{ref}} \times 1\,000\,000}{P_{\text{fwd}}}}$$

$$\text{SWR} = \frac{1000 + (\Gamma \times 1000)}{1000 - (\Gamma \times 1000)}$$

- If $P_{\text{fwd}} \le 0$: returns 1.00 (idle).
- If $P_{\text{ref}} \ge P_{\text{fwd}}$: returns 9.99 (high SWR / open / short).
- If $P_{\text{ref}} = 0$: returns 1.00.

---

## Directory Layout

```text
swr-stm32f4-cmake/
├── CMakeLists.txt              # Build configuration
├── arm-none-eabi.cmake         # Toolchain file for ARM GCC
├── LinkerScript.ld             # Linker script for STM32F401xC (with .noinit RAM section)
├── README.md
├── LICENSE                     # PolyForm Noncommercial 1.0.0
├── PCB and Schematic/          # Target hardware files (STC8)
│   ├── SCH_Schematic1_2026-09-30.pdf # Schematic
│   ├── Gerber/                 # 4-layer production Gerbers
│   ├── front.jpeg              # 3D render (front)
│   ├── front no 3d.jpeg        # PCB layout (front)
│   ├── back.jpeg               # 3D render (back)
│   └── back no 3d.jpeg         # PCB layout (back)
└── Core/
    ├── Inc/                    # Header files
    │   ├── adc.h
    │   ├── button.h
    │   ├── buzzer.h
    │   ├── calibration.h
    │   ├── conversion.h
    │   ├── lcd.h
    │   ├── menu.h
    │   ├── menu_calibration.h
    │   ├── menu_diagnostic.h
    │   ├── menu_main_screen.h
    │   ├── menu_protection.h
    │   ├── protection.h
    │   ├── storage.h
    │   ├── sytick.h
    │   └── watchdog.h
    └── Src/                    # Implementation
        ├── adc.c               # ADC1 register-level driver
        ├── button.c            # Debounced button inputs & hold detection
        ├── buzzer.c            # Buzzer driver (PB0 on proto, P2.6 on STC8)
        ├── calibration.c       # Calibration CRUD, sorting, and curve scaling
        ├── conversion.c        # SWR integer math & EMA filter
        ├── lcd.c               # I2C LCD driver & custom glyphs
        ├── main.c              # Entry point, watchdog tracking & main loop
        ├── menu.c              # Main menu coordinator & LCD helpers
        ├── menu_calibration.c  # Multi-point calibration & adapt workflow
        ├── menu_diagnostic.c   # Hardware diagnostic & self-test routines
        ├── menu_main_screen.c  # Home measurement screen & power bars
        ├── menu_protection.c   # Protection thresholds & relay controls
        ├── protection.c        # Protection trip engine & relay control
        ├── storage_stm32.c     # Flash Sector 5 persistence
        ├── sytick.c            # Delay functions
        ├── watchdog.c          # IWDG driver & .noinit crash breadcrumbs
        ├── startup_stm32f401xc.s
        └── system_stm32f4xx.c
```

---

## Building and Flashing

### Requirements
- `arm-none-eabi-gcc` toolchain
- CMake (>= 3.20) and Ninja (or Make)
- `stlink-tools` (`st-flash`) or OpenOCD

### Build
```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
cmake --build build
```

### Flash
```bash
cmake --build build --target flash
# or using st-flash directly:
st-flash --reset write build/swr-stm32f4-cmake.bin 0x08000000
```

---

## License

This project is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). Free for personal, hobbyist, and educational use. Commercial use is not permitted without permission.
