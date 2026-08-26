# BOM: Nucleo F446RE Induction Heater PLL Shield (Prototype)

Digital PLL controller for induction heater using STM32 Nucleo-F446RE.
TIM1 complementary PWM with hardware dead-time and break input.
Galvanically isolated signal path between controller and power stage.

---

## Core Board

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | STM32 Nucleo-F446RE | NUCLEO-F446RE | Dev board | Controller | |
| 2 | Pin header female | 2x38 pin, 2.54mm | Through-hole | Morpho socket | Nucleo plugs into these |

## Signal Isolation (Critical)

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | Digital isolator | Si8621BB-B-IS | SOIC-8 (narrow) | PWM_A + PWM_B output isolation | 150Mbps, <8ns prop delay. Alt: ADUM1201 (DIP available) |
| 1 | Digital isolator | Si8621BB-B-IS | SOIC-8 | FAULT_IN + FREQ feedback isolation | Same part, second unit |

> If SOIC is too difficult for perfboard, the ADUM1201BRZ breakout boards are available
> on Amazon/AliExpress for a few bucks. Solder the breakout module in as a daughter board.

## Protection

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 4 | TVS diode | 3.3V unidirectional | Through-hole or SOD-123 | Clamp on PWM_A, PWM_B, FAULT, FREQ | PESD3V3L1BA or any 3.3V TVS |
| 4 | Resistor | 100R 1/4W | Through-hole | Series on output signals (PWM_A, PWM_B) and inputs | Current limiting |
| 2 | Resistor | 1kR 1/4W | Through-hole | Series on input signals (FAULT, FREQ) | Current limiting |
| 2 | Schottky diode | BAT54 or 1N5819 | Through-hole | Clamp to 3.3V rail on inputs | Overvoltage protection |

## Power Supply

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | Capacitor | 100uF 10V electrolytic | Through-hole | Bulk decoupling 5V input | |
| 4 | Capacitor | 100nF ceramic | Through-hole | Decoupling on isolators + Nucleo VIN | |
| 1 | Capacitor | 10uF ceramic or electrolytic | Through-hole | Isolator Vdd2 side bulk | |

> Power: Nucleo takes 5V on VIN pin (CN7 pin 18) from existing 5V rail.
> Isolators need power on both sides: 3.3V from Nucleo on side 1, 5V from
> driver rail on side 2.

## User Interface

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | TFT display | 1.8" ST7735S 128x160 | Module w/ header | Status display | Same as ESP32 version |
| 1 | Rotary encoder | EC11 w/ push button | Through-hole | Frequency/dead-time adjust | |
| 1 | Capacitor | 100nF ceramic | Through-hole | Encoder hardware debounce | Between encoder pins and GND |
| 2 | Resistor | 10kR 1/4W | Through-hole | Encoder pull-ups (optional if using internal) | |
| 1 | Tactile button | 6mm push button | Through-hole | Start/stop/reset | |
| 1 | Resistor | 10kR 1/4W | Through-hole | Button pull-up | |
| 1 | LED | Green 3mm | Through-hole | Status/heartbeat | |
| 1 | LED | Red 3mm | Through-hole | Fault indicator | |
| 2 | Resistor | 330R 1/4W | Through-hole | LED current limit | |

## Analog Inputs

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | NTC Thermistor | 10kR @ 25C (B=3435) | Through-hole | Temperature monitoring | NTCLE100E3103 or equivalent |
| 1 | Resistor | 10kR 1/4W 1% | Through-hole | NTC voltage divider | |
| 2 | Resistor | TBD (based on bus voltage) | Through-hole | Bus voltage divider (scale to 0-3.3V) | Calculate for your V_bus |
| 2 | Capacitor | 10nF ceramic | Through-hole | ADC anti-aliasing filter | On NTC and bus voltage inputs |

## Connectors

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | Screw terminal | 2-pos 5.08mm | Through-hole | 5V power input | |
| 1 | Screw terminal | 2-pos 5.08mm | Through-hole | 15V power input | Gate drive supply |
| 1 | Screw terminal | 2-pos 5.08mm | Through-hole | GDT output leg A | To GDT primary |
| 1 | Screw terminal | 2-pos 5.08mm | Through-hole | GDT output leg B | To GDT primary |
| 1 | Screw terminal | 2-pos 5.08mm | Through-hole | FAULT input (from OCP comparator) | |
| 1 | Screw terminal | 2-pos 5.08mm | Through-hole | FREQ feedback input | |
| 1 | Screw terminal | 2-pos 5.08mm | Through-hole | NTC thermistor | |
| 1 | Pin header male | 8-pin 2.54mm | Through-hole | TFT module connector | |

## Frequency Feedback Signal Conditioning (power-side, before Si8621 #2)

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | 74HC14 | Schmitt trigger inverter | DIP-14 | Convert CT sine to clean square wave | Only 1 of 6 gates used |
| 1 | Resistor | 10kΩ 1/4W | Through-hole | Voltage divider upper (CT → HC14 input) | |
| 1 | Resistor | 15kΩ 1/4W | Through-hole | Voltage divider lower (HC14 input → GND) | |
| 1 | Capacitor | 100nF ceramic | Through-hole | 74HC14 VCC decoupling | Close to pin 14 |

## Gate Driver Section (15V rail, power-side GND)

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 2 | IXDN604 | IXDN604PI | TO-220-5 | Non-inverting gate driver (9A peak) | Both same type, no IXDI needed |
| 2 | Capacitor | 100nF ceramic | Through-hole | IXDN VCC decoupling (close to pin 1) | |
| 2 | Capacitor | 10µF electrolytic | Through-hole | IXDN VCC bulk (near each chip) | 25V rated |
| 2 | Resistor | 10kΩ 1/4W | Through-hole | Pulldown on IXDN IN (pin 4) | Keeps gates OFF if signal absent |

## Prototype Board

| Qty | Part | Value/Type | Package | Purpose | Notes |
|-----|------|-----------|---------|---------|-------|
| 1 | Perfboard/stripboard | ~100x80mm | -- | Base board | Sized to fit Nucleo + components |

---

## Pin Assignments (F446RE Morpho Headers)

| Function | STM32 Pin | Timer/Peripheral | Morpho Header |
|----------|-----------|-----------------|---------------|
| PWM_A | PA8 | TIM1_CH1 | CN10 pin 23 |
| PWM_B | PB13 | TIM1_CH1N | CN10 pin 30 |
| FAULT_IN | PB12 | TIM1_BKIN | CN10 pin 16 |
| FREQ_FB | PA0 | TIM2_CH1 (Input Capture) | CN7 pin 28 |
| ADC_OCP | PA1 | ADC1_IN1 | CN7 pin 30 |
| ADC_VBUS | PA4 | ADC1_IN4 | CN7 pin 32 |
| ADC_NTC | PC0 | ADC1_IN10 | CN7 pin 38 |
| ENC_A | PB4 | TIM3_CH1 | CN10 pin 27 |
| ENC_B | PB5 | TIM3_CH2 | CN10 pin 29 |
| ENC_BTN | PC13 | GPIO (EXTI) | CN7 pin 23 |
| TFT_MOSI | PA7 | SPI1_MOSI | CN10 pin 15 |
| TFT_SCK | PA5 | SPI1_SCK | CN10 pin 11 |
| TFT_CS | PB6 | GPIO | CN10 pin 17 |
| TFT_DC | PC7 | GPIO | CN10 pin 19 |
| TFT_RST | PA9 | GPIO | CN10 pin 21 |
| STATUS_LED | PB0 | GPIO | CN7 pin 34 |
| FAULT_LED | PB1 | GPIO | CN7 pin 7 |

---

## Order Priority

**Likely need to order:**
- 2x Si8621BB-B-IS or ADUM1201 isolators (or breakout boards)
- 4x 3.3V TVS diodes (if not in stock)
- 2x 38-pin female headers (Nucleo morpho)

**Likely have in stock:**
- Resistors, capacitors, LEDs
- Rotary encoder, TFT display, buttons
- Screw terminals, pin headers, wire
- NTC thermistor
- Perfboard
