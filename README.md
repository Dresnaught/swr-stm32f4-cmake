# Digital SWR & Power Meter

Digital SWR and RF power meter designed primarily for monitoring VHF power amplifiers / boosters (144 MHz / 2m band).

The firmware is currently running on an **STM32F401xC** ("Black Pill") for prototyping and algorithm tuning. The final target hardware is based on an **STC8H8K64U** microcontroller; schematic and 4-layer Gerber files are available in `PCB and Schematic/`. All core math, calibration, menu logic, and LCD rendering are written in portable C to make porting to 8051 (SDCC / Keil C51) straightforward.

---

## Features

- **RF Measurements**: 3-channel 12-bit ADC sensing for Forward (`FWD`), Reflected (`REF`), and Radio Drive (`RAD`) voltages.
- **Diode Non-Linearity Compensation**: Piecewise linear interpolation (PWLI) with up to 10 user-defined calibration points per channel.
- **Fast Integer Math**: SWR and power calculations use integer arithmetic and a fast integer square root (`isqrt`), keeping execution fast without floating-point overhead.
- **Signal Filtering**: Exponential moving average (EMA) filter on ADC readings to smooth out mains noise and RF jitter without introducing noticeable lag.
- **Tuned Display Refresh**: Main screen updates every 180 ms (~5.5 Hz) to match standard HD44780 LCD response times and prevent ghosting.
- **Power Bar Options**:
  - Custom tick scale (`''''|`) using 8 custom LCD CGRAM glyphs (1px ticks, half-ticks, full division marks).
  - Solid progressive bar (`|||||`).
  - Off (shows REF or RAD numeric wattage on line 2).
  - Quick toggle using UP / DOWN buttons on the home screen.
- **Hardware Protection**:
  - High SWR cutoff (configurable from 1.1 to 5.0, only trips when FWD power is present).
  - Over-drive cutoff (RAD power limit from 1W to 50W).
  - Drives an optocoupler + relay (`PB2`) to drop transmitter PTT or insert an attenuator on fault.
  - Fault lock screen shows reason (`*TRIP* HI SWR!` or `*TRIP* HI RAD!`). Press SELECT to reset, or hold SELECT (500ms) / UP / DOWN to go to the menu.
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
- **Watchdog Protection**: Hardware IWDG enabled to recover automatically in case of severe RF/EMI lockup.
- **Debounced Dual-Action Buttons**:
  - Release-based short press prevents unintended triggers when pressing and holding.
  - 500 ms long hold triggers immediately.

---

## Project Status & Roadmap

| Item | Platform | Status |
| :--- | :--- | :--- |
| Core drivers (ADC, I2C, SysTick, GPIO) | STM32F401xC | Working |
| SWR math, PWLI interpolation, EMA filter | Portable C | Working |
| HD44780 LCD driver & custom bar characters | Portable C | Working |
| Button debouncing & long-press handling | STM32 / Portable | Working |
| On-device multi-point calibration & Flash storage | Portable C / STM32 | Working |
| Protection limits & relay cutoff logic | Portable C | Working |
| Display modes (Scale bar, Pipe bar, Text) | Portable C | Working |
| Standby marquee screensaver | Portable C | Working |
| Hardware IWDG watchdog | STM32F401xC | Working |
| STC8 PCB schematic & 4-layer Gerbers | Hardware | Complete (`PCB and Schematic/`) |
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
| **Buzzer** | — | `P2.6` | Buzzer on trip (target PCB) |
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
  │     ├── 1. Add Point    -> Enter target watts -> apply RF -> 5s sample -> save
  │     ├── 2. Edit Point   -> Pick point -> edit watts -> sample
  │     ├── 3. Remove Point -> Pick point -> confirm delete
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
├── LinkerScript.ld             # Linker script for STM32F401xC
├── README.md
├── LICENSE                     # PolyForm Noncommercial 1.0.0
├── PCB and Schematic/          # Target hardware files (STC8)
│   ├── SCH_Schematic1_2026-09-30.pdf # Schematic
│   └── Gerber/                 # 4-layer production Gerbers
└── Core/
    ├── Inc/                    # Header files
    │   ├── adc.h
    │   ├── button.h
    │   ├── calibration.h
    │   ├── conversion.h
    │   ├── lcd.h
    │   ├── menu.h
    │   ├── protection.h
    │   ├── storage.h
    │   ├── sytick.h
    │   └── watchdog.h
    └── Src/                    # Implementation
        ├── adc.c               # ADC1 register-level driver
        ├── button.c            # Debounced button inputs & hold detection
        ├── calibration.c       # Calibration CRUD, sorting, and checksums
        ├── conversion.c        # SWR integer math & EMA filter
        ├── lcd.c               # I2C LCD driver & custom glyphs
        ├── main.c              # Entry point & main loop
        ├── menu.c              # UI menu logic & display routines
        ├── protection.c        # Protection trip engine & relay control
        ├── storage_stm32.c     # Flash Sector 5 persistence
        ├── sytick.c            # Delay functions
        ├── watchdog.c          # IWDG initialization and refresh
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
