# Nucleo-64 Pinout Reference (F411RE / F446RE)

The Nucleo-64 form factor is identical between F411RE and F446RE.
This documents the connector layout, pin numbers, and functions.

## Connector Overview

The board has TWO physical rows on each side, but they are electrically the same nets:

- **CN7** (Morpho, left side) — CN6 + CN8 (Arduino headers, left) tap the SAME nets
- **CN10** (Morpho, right side) — CN5 + CN9 (Arduino headers, right) tap the SAME nets

So an Arduino header pin (e.g. D13) is directly wired to a specific Morpho pin.
The Morpho headers are just the "full access" version of the Arduino headers plus
all the extra MCU pins the Arduino form factor doesn't expose.

```
        ┌────────────────────────────────────────┐
        │  CN7 (Morpho L)      CN10 (Morpho R)    │
        │  ▪▪ 2 rows           2 rows ▪▪          │
        │                                          │
        │  CN8/CN6             CN5/CN9             │
        │  (Arduino L)         (Arduino R)         │
        │                                          │
        │           [ST-LINK / USB]                │
        └────────────────────────────────────────┘
```

## Arduino Headers (Right side — CN5 top, CN9 lower)

| Arduino | MCU Pin | Function (default AF) | Notes |
|---------|---------|----------------------|-------|
| D15 | PB8 | I2C1_SCL | |
| D14 | PB9 | I2C1_SDA | |
| AREF | - | Analog reference | |
| GND | - | Ground | |
| D13 | PA5 | SPI1_SCK / **User LED (green)** | LED_BUILTIN |
| D12 | PA6 | SPI1_MISO | |
| D11 | PA7 | SPI1_MOSI / TIM1_CH1N | |
| D10 | PB6 | TIM4_CH1 / I2C1_SCL | |
| D9  | PC7 | TIM3_CH2 | |
| D8  | PA9 | USART1_TX / TIM1_CH2 | |

| Arduino | MCU Pin | Function (default AF) | Notes |
|---------|---------|----------------------|-------|
| D7  | PA8 | TIM1_CH1 | |
| D6  | PB10 | TIM2_CH3 | |
| D5  | PB4 | TIM3_CH1 | |
| D4  | PB5 | TIM3_CH2 | |
| D3  | PB3 | TIM2_CH2 | |
| D2  | PA10 | USART1_RX / TIM1_CH3 | |
| D1  | PA2 | USART2_TX (ST-Link VCP) | Serial console |
| D0  | PA3 | USART2_RX (ST-Link VCP) | Serial console |

## Arduino Headers (Left side — CN6 top, CN8 lower)

### CN6 — Power

| Pin | Function | Notes |
|-----|----------|-------|
| NC | - | |
| IOREF | 3.3V ref | |
| RESET | NRST | |
| +3V3 | 3.3V out | |
| +5V | 5V out/in | |
| GND | Ground | |
| GND | Ground | |
| VIN | 7-12V input | External power (E5V jumper) |

### CN8 — Analog

| Arduino | MCU Pin | Function | Notes |
|---------|---------|----------|-------|
| A0 | PA0 | ADC1_IN0 / TIM2_CH1 | |
| A1 | PA1 | ADC1_IN1 / TIM2_CH2 | |
| A2 | PA4 | ADC1_IN4 / SPI1_NSS | |
| A3 | PB0 | ADC1_IN8 / TIM3_CH3 | |
| A4 | PC1 | ADC1_IN11 | |
| A5 | PC0 | ADC1_IN10 | |

## Morpho Headers — Full Pin Access

### CN7 (Left Morpho) — Pin numbers 1-38

```
        CN7 (odd = left col, even = right col)
   ┌──────────────────────────────────────────┐
 1 │ PC10          PC11 │ 2
 3 │ PC12          PD2  │ 4
 5 │ VDD           E5V  │ 6
 7 │ BOOT0         GND  │ 8
 9 │ NC            NC   │ 10
11 │ NC            IOREF│ 12
13 │ PA13          RESET│ 14
15 │ PA14          +3V3 │ 16
17 │ PA15          +5V  │ 18
19 │ GND           GND  │ 20
21 │ PB7           GND  │ 22
23 │ PC13          VIN  │ 24
25 │ PC14          NC   │ 26
27 │ PC15          PA0  │ 28
29 │ PH0           PA1  │ 30
31 │ PH1           PA4  │ 32
33 │ VBAT          PB0  │ 34
35 │ PC2           PC1  │ 36
37 │ PC3           PC0  │ 38
   └──────────────────────────────────────────┘
```

### CN10 (Right Morpho) — Pin numbers 1-38

```
        CN10 (odd = left col, even = right col)
   ┌──────────────────────────────────────────┐
 1 │ PC9           PC8  │ 2
 3 │ PB8           PC6  │ 4
 5 │ PB9           PC5  │ 6
 7 │ AVDD          U5V  │ 8
 9 │ GND           NC   │ 10
11 │ PA5           PA12 │ 12
13 │ PA6           PA11 │ 14
15 │ PA7           PB12 │ 16
17 │ PB6           PB11 │ 18
19 │ PC7           GND  │ 20
21 │ PA9           PB2  │ 22
23 │ PA8           PB1  │ 24
25 │ PB10          PB15 │ 26
27 │ PB4           PB14 │ 28
29 │ PB5           PB13 │ 30
31 │ PB3           AGND │ 32
33 │ PA10          PC4  │ 34
35 │ PA2           NC   │ 36
37 │ PA3           NC   │ 38
   └──────────────────────────────────────────┘
```

## Project Pin Assignments — Where They Land

| Function | MCU Pin | Morpho Location | Arduino Alias |
|----------|---------|-----------------|---------------|
| PWM_A | PA8 | CN10 pin 23 | D7 |
| PWM_B | PB13 | CN10 pin 30 | - |
| FAULT_IN (BKIN) | PB12 | CN10 pin 16 | - |
| FREQ_FB | PA0 | CN7 pin 28 | A0 |
| ADC_OCP | PA1 | CN7 pin 30 | A1 |
| ADC_VBUS | PA4 | CN7 pin 32 | A2 |
| ADC_NTC | PC0 | CN7 pin 38 | A5 |
| AC_SENSE | PC1 | CN7 pin 36 | A4 |
| ENC_A | PB4 | CN10 pin 27 | D5 |
| ENC_B | PB5 | CN10 pin 29 | D4 |
| ENC_BTN | PC13 | CN7 pin 23 | - |
| TFT_SCK | PA5 | CN10 pin 11 | D13 |
| TFT_MOSI | PA7 | CN10 pin 15 | D11 |
| TFT_CS | PB6 | CN10 pin 17 | D10 |
| TFT_DC | PC7 | CN10 pin 19 | D9 |
| TFT_RST | PA9 | CN10 pin 21 | D8 |
| STATUS_LED | PB0 | CN7 pin 34 | A3 |
| CONTACTOR | PB14 | CN10 pin 28 | - |

## Pins to AVOID

| Pin | Why |
|-----|-----|
| PA13, PA14 | SWD debug (SWDIO/SWCLK) — used by ST-Link |
| PA5 | Also drives onboard green LED — OK for TFT SCK but LED will flicker |
| PB3, PA15, PB4 | JTAG remnants — usable but need remap from default |
| PC14, PC15 | 32kHz crystal (if RTC used) |
| PH0, PH1 | Main oscillator (if HSE used) |
| BOOT0 | Boot mode select — do not pull high at reset |
| PA2, PA3 | USART2 → ST-Link virtual COM port (serial console) |

> Note: This is the physical/electrical layout common to F411RE and F446RE.
> The default alternate-function (AF) mappings shown are the most common;
> some pins have multiple AF options selectable in firmware.
> Verify against the specific datasheet if using a non-default AF.
